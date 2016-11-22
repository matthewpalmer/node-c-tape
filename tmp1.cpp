// addon.cc
#include <node.h>
// #include "ArrayBuffer.h"
#include <node_buffer.h>
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
  uv_cond_t cond;
  bool done;
  Persistent<Function> callback;
  std::string *tape;
  std::string *input;
  std::string *output;
};

std::list<Work *> internalBuffer;
pthread_mutex_t internalMutex;
pthread_cond_t full;
sem_t *jsonOpsEmpty;

void PlayTape(std::string *tape, rapidjson::Document *d) {
  std::vector<Command> commands = json_asm::tokenize(tape);

  for (auto const& command: commands) {
    void *result = json_asm::execute(command, d);
  }
}

void JsonTaskDone(uv_work_t *req) {
    // std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    Work *work = (Work *)req->data;

    rapidjson::Document d;

    if (d.Parse(work->input->c_str()).HasParseError()) {
      printf("Parse error in json-tape, error: %d, position: %zu\n", d.GetParseError(), d.GetErrorOffset());
      // return NULL;
    }
    
    PlayTape(work->tape, &d);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    d.Accept(writer);
    const char* result = buffer.GetString();

    // uv_mutex_lock(&work->mutex);
    work->output = new std::string(result);
    work->done = true;
  
  // std::chrono::steady_clock::time_point end= std::chrono::steady_clock::now();
  // std::cout << "json task done Time difference = " << std::chrono::duration_cast<std::chrono::microseconds> (end - begin).count() << std::endl;
}

void createbuffer(char *data, int length) {

  // node::ArrayBuffer *buffer = node::ArrayBuffer::New(isolate, "Hello World!");
  

  // args.GetReturnValue().Set(buffer->ToArrayBuffer());

  // memcpy(Buffer::Data(slowBuffer), data, length);

  // v8::Local<v8::Object> globalObj = isolate->GetCurrentContext()->Global();
  // v8::Local<v8::Function> bufferConstructor = v8::Local<v8::Function>::Cast(globalObj->Get(v8::String::NewFromUtf8(isolate, "Buffer")));
  // v8::Handle<v8::Value> constructorArgs[3] = { slowBuffer->handle_, v8::Integer::New(length), v8::Integer::New(0) };
  // v8::Local<v8::Object> actualBuffer = bufferConstructor->NewInstance(3, constructorArgs);
  // return scope.Close(actualBuffer);
}

void JsonTaskDoneDone(uv_work_t *req, int code) {
std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

  Isolate *isolate = Isolate::GetCurrent();

  // v8::EscapableHandleScope handle_scope(isolate);
  // printf("got isolate %p\n", isolate);
  v8::HandleScope handleScope(isolate);

  Work *work = static_cast<Work *>(req->data);

  // Restore the output data from the heap to the main thread's stack as V8 objects
  // Handle<Value> result = String::NewFromUtf8(isolate, work->output->c_str());
  // String::NewFromUtf8(isolate, work->output->c_str(), v8::NewStringType::kInternalized);


  
  v8::MaybeLocal<v8::Object> o = node::Buffer::New(isolate, (char *)work->output->c_str(), work->output->length());

  // Call back to Node with our result
  // Handle<v8::Object> result = v8::Handle<v8::Object>(o.val_);

  Handle<Value> argv[] = { o.ToLocalChecked() };
  Local<Function>::New(isolate, work->callback)->Call(isolate->GetCurrentContext()->Global(), 1, argv);
  work->callback.Reset();

  // delete work->input;
  // delete work->output;
  // delete work; 


  std::chrono::steady_clock::time_point end= std::chrono::steady_clock::now();
  std::cout << "json task done done done done Time difference = " << std::chrono::duration_cast<std::chrono::microseconds> (end - begin).count() << std::endl;
}

// Kick off a JSON task. Called on a new (non-uv) thread.
void *JsonTask(void *data) {
  std::thread::id this_id = std::this_thread::get_id();
     std::cout << "json task thread " << this_id << " sleeping...\n";

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

  // uv_mutex_lock(&work->mutex);
  work->output = new std::string(result);
  work->done = true;

  // uv_cond_signal(&work->cond);
  // uv_mutex_unlock(&work->mutex);

  // uv_queue_work(uv_default_loop(), &work->request, JsonTaskDoneDone, JsonTaskDone);

  return NULL;
}


// Creates a new pthread and waits for it to complete.
// Should be called from a uv worker thread.
void NewThreadAndWaitOnWorker(uv_work_t *req) {
// printf("Going to do work and wait\n");
  Work *work = static_cast<Work *>(req->data);
  work->done = false;

  // Run the JSON operations on a new thread and block until they complete
  pthread_t t;
  pthread_create(&t, NULL, &JsonTask, (void *)work);

  uv_mutex_lock(&work->mutex);

  while (!work->done) { // Handle spurious wakeups
    uv_cond_wait(&work->cond, &work->mutex);  
  }

  uv_mutex_unlock(&work->mutex);

  // JSON operations finished. 
  // When this function ends libuv will call our callback automatically.
}

// Callback for libuv when our JSON task completes.
void NewThreadAndWaitOnWorkerDone(uv_work_t *req, int status) {
  // cout << "Going to call back" << endl;
  Isolate *isolate = Isolate::GetCurrent();
  v8::HandleScope handleScope(isolate);

  Work *work = static_cast<Work *>(req->data);

  // Restore the output data from the heap to the main thread's stack as V8 objects
  Handle<Value> result = String::NewFromUtf8(isolate, work->output->c_str());

  // Call back to Node with our result
  Handle<Value> argv[] = { };
  Local<Function>::New(isolate, work->callback)->Call(isolate->GetCurrentContext()->Global(), 1, argv);
  work->callback.Reset();

  delete work->input;
  delete work->output;
  delete work;

  sem_post(jsonOpsEmpty);
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

  Work *work = internalBuffer.front();
  internalBuffer.pop_front();
  pthread_mutex_unlock(&internalMutex);
  return work;
}

void ForeverRunQueue(uv_work_t *req) {
  while (1) {
    // cout << "waiting on json" << endl;
    sem_wait(jsonOpsEmpty);

    // cout << "About to take from queue" << endl;
    Work *next = TakeInternalQueue();
    // cout << "Took from queue" << endl;

    // cout << "Adding stuff to worker" << endl;
    
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
  
  uv_cond_init(&work->cond);
  uv_mutex_init(&work->mutex);

  work->done = false;

  // pthread_t t;
  // pthread_create(&t, NULL, &JsonTask, (void *)work);

  // Move to a worker thread that will then kick off the JSON operations in the background.
  //PushInternalQueue(work);
  uv_queue_work(uv_default_loop(), &work->request, JsonTaskDone, JsonTaskDoneDone);
  //JsonTaskDone(&work->request);

  // v8::String s = new v8::String(work->output);
  // node::Buffer b = new node::Buffer::new(work->output->length());

  // Handle<v8::String> out = v8::internal::String::Flatten(work->output, );

  // Return to Node.
  // Handle<Value> result = String::NewFromOneByte(isolate, (uint8_t *)(work->output->c_str()), work->output->length());
  args.GetReturnValue().Set(Undefined(isolate));
}

void Init(Local<Object> exports) {
  // uv_work_t request;
  // pthread_mutex_init(&internalMutex, NULL);
  // pthread_cond_init(&full, NULL);

  // const char *name = "/json-tape";

  // jsonOpsEmpty = sem_open(name, O_CREAT | O_EXCL, S_IRWXU | S_IRWXG, 2);
  // if (jsonOpsEmpty == SEM_FAILED) {
  //   printf("json-tape failed to get a sempahore\n");
  // } else {
  //   sem_unlink("/json-tape");
  // }

  // printf("jsonOpsEmpty %p\n", jsonOpsEmpty);

  // uv_queue_work(uv_default_loop(), &request, ForeverRunQueue, ForeverHasEnded);

  NODE_SET_METHOD(exports, "play", Play);
}

NODE_MODULE(addon, Init)

}
