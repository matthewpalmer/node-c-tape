// addon.cc
#include <node.h>
#include "deps/rapidjson/include/rapidjson/document.h"
#include "deps/rapidjson/include/rapidjson/writer.h"
#include "deps/rapidjson/include/rapidjson/stringbuffer.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <uv.h>
#include <pthread.h>
#include <string>
#include <sstream>
#include <vector>
#include <list>
#include <semaphore.h>

#include <unistd.h>

#include "command.hpp"
#include "json-asm.hpp"

using namespace std;

namespace json_tape {

using v8::Exception;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Persistent;
using v8::Handle;

// TODOs
// - convert our js json-taper to cpp
// - split to separate files
// - make sure we are cleaning up threads properly
// - are we doing any unnecessary string copies on non-bg threads?
// - Callbacks stack up... is this expected?
// - memory usage grows over time... why is that happening + fix it

struct Work {
  uv_work_t request;
  uv_mutex_t mutex;
  uv_async_t async;
  uv_cond_t cond;
  bool done;
  Persistent<Function> callback;
  std::string *tape;
  std::string *input;
  std::string *output;
};

uv_loop_t *loop;
std::list<Work *> internalBuffer;
pthread_mutex_t internalMutex;
pthread_cond_t full;
sem_t jsonRunning;

void PlayTape(std::string *tape, rapidjson::Document *d) {
  std::vector<Command> commands = json_asm::tokenize(tape);

  for (auto const& command: commands) {
    void *result = json_asm::execute(command, d);
  }
}

// Kick off a JSON task. Called on a new (non-uv) thread.
void *JsonTask(void *data) {
  Work *work = (Work *)data;

  rapidjson::Document d;

  if (d.Parse(work->input->c_str()).HasParseError()) {
    printf("Parse error in json-tape, error: %d, position: %zu\n", d.GetParseError(), d.GetErrorOffset());
    return NULL;
  }
  
  PlayTape(work->tape, &d);

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  d.Accept(writer);
  const char* result = buffer.GetString();

  uv_mutex_lock(&work->mutex);
  work->output = new std::string(result);
  work->done = true;

  uv_cond_signal(&work->cond);
  uv_mutex_unlock(&work->mutex);

  return NULL;
}

// Creates a new pthread and waits for it to complete.
// Should be called from a uv worker thread.
void NewThreadAndWaitOnWorker(uv_work_t *req) {
  cout << "Spinning up new thread for json op" << endl;

  Work *work = static_cast<Work *>(req->data);
  work->done = false;

  // Run the JSON operations on a new thread and block until they complete
  pthread_t t;
  pthread_create(&t, NULL, &JsonTask, (void *)work);

  uv_mutex_lock(&work->mutex);

  while (!work->done) { // Handle spurious wakeups
    uv_cond_wait(&work->cond, &work->mutex);  
  }

  // JSON operations finished. 
  // When this function ends libuv will call our callback automatically.
  cout << "Worker is returning..." << endl;
  work->async.data = static_cast<void *>(work);
  uv_async_send(&work->async);

  uv_mutex_unlock(&work->mutex);
}

// Callback for libuv when our JSON task completes.
void NewThreadAndWaitOnWorkerDone(uv_async_t *handle) {
  cout << "And worker done callback called" << endl;
  Isolate *isolate = Isolate::GetCurrent();
  v8::HandleScope handleScope(isolate);
  Work *work = static_cast<Work *>(handle->data);

  uv_mutex_lock(&work->mutex);

  // Restore the output data from the heap to the main thread's stack as V8 objects
  Handle<Value> result = String::NewFromUtf8(isolate, work->output->c_str());

  // Call back to Node with our result
  Handle<Value> argv[] = { result };
  Local<Function>::New(isolate, work->callback)->Call(isolate->GetCurrentContext()->Global(), 1, argv);
  work->callback.Reset();

  uv_mutex_unlock(&work->mutex);

  delete work->input;
  delete work->output;
  delete work;

  sem_post(&jsonRunning);
}

void PushInternalQueue(Work *work) {
  pthread_mutex_lock(&internalMutex);
  bool previouslyEmpty = internalBuffer.size() == 0;
  internalBuffer.push_back(work);
  pthread_cond_signal(&full);
  pthread_mutex_unlock(&internalMutex);
}

Work *TakeInternalQueue() {
  pthread_mutex_lock(&internalMutex);
  pthread_cond_wait(&full, &internalMutex);
  sem_wait(&jsonRunning);
  Work *work = internalBuffer.front();
  internalBuffer.pop_front();
  pthread_mutex_unlock(&internalMutex);
  return work;
}

void ForeverRunQueue(uv_work_t *req) {
  pthread_mutex_init(&internalMutex, NULL);
  pthread_cond_init(&full, NULL);
  sem_init(&jsonRunning, 0, 1);

  while (1) {
    cout << "About to take from internal queue" << endl;
    Work *next = TakeInternalQueue();
    
    // uv_async_init(loop, &work->async, JsCallback);
    // uv_queue_work(loop, &work->request, NewThreadAndWaitOnWorker, Cleanup);

    NewThreadAndWaitOnWorker(&next->request);
    // NewThreadAndWaitOnWorkerDone(&next->request, 0);
    // cout << "About to submit to worker queue" << endl;
    // uv_queue_work(uv_default_loop(), &next->request, NewThreadAndWaitOnWorker, NewThreadAndWaitOnWorkerDone);
  }
}

void ForeverHasEnded(uv_work_t *req, int status) {
  // no.
}

// Entrypoint. Called on the main thread.
void Play(const FunctionCallbackInfo<Value>& args) {
  Isolate *isolate = args.GetIsolate();

  if (args.Length() < 3) {
    isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, "Incorrect number of arguments.")));
    return;
  }

  if (!args[0]->IsString() || !args[1]->IsString() || !args[2]->IsFunction()) {
    isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, "Invalid argument format. Needs (string, string, callback).")));
    return;
  }

  Work *work = new Work();
  work->request.data = work;

  // Copy input strings to heap
  String::Utf8Value inputV8Str(args[0]->ToString());
  std::string *inputStr = new std::string(*inputV8Str);
  work->input = inputStr;

  String::Utf8Value tapeV8Str(args[1]->ToString());
  std::string *tapeStr = new std::string(*tapeV8Str);
  work->tape = tapeStr;

  // Save the callback
  Local<Function> callback = Local<Function>::Cast(args[2]);
  work->callback.Reset(isolate, callback);
  
  loop = uv_default_loop();
  uv_async_init(loop, &work->async, NewThreadAndWaitOnWorkerDone);

  uv_cond_init(&work->cond);
  uv_mutex_init(&work->mutex);

  // Move to a worker thread that will then kick off the JSON operations in the background.
  cout << "Queueing up work..." << endl;
  PushInternalQueue(work);

  // Return to Node.
  args.GetReturnValue().Set(Undefined(isolate));
}

void Init(Local<Object> exports) {
  uv_work_t request;
  uv_queue_work(uv_default_loop(), &request, ForeverRunQueue, ForeverHasEnded);

  NODE_SET_METHOD(exports, "play", Play);
}

NODE_MODULE(addon, Init)

}
