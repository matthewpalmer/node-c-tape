// addon.cc
#include <node.h>
#include <node_buffer.h>
#include "deps/rapidjson/include/rapidjson/document.h"
#include "deps/rapidjson/include/rapidjson/writer.h"
#include "deps/rapidjson/include/rapidjson/stringbuffer.h"
#include <iostream>
#include <uv.h>
#include <pthread.h>

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

struct Work {
  uv_work_t request;
  Persistent<Function> callback;
  unsigned char *outBuffer;
  std::string *tape;
  std::string *input;
  std::string *output;
};

void PlayTape(std::string *tape, rapidjson::Document *d) {
  std::vector<Command> commands = json_asm::tokenize(tape);

  for (auto const& command: commands) {
    void *result = json_asm::execute(command, d);
  }
}

void StartJsonTask(uv_work_t *req) {
    Work *work = (Work *)req->data;

    rapidjson::Document d;

    if (d.Parse(work->input->c_str()).HasParseError()) {
      printf("Parse error in json-tape, error: %d, position: %zu\n", d.GetParseError(), d.GetErrorOffset());
      return;
    }
    
    PlayTape(work->tape, &d);

    rapidjson::StringBuffer stringBuffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);
    d.Accept(writer);
    const char* result = stringBuffer.GetString();

    // void * memcpy ( void * destination, const void * source, size_t num );
    memcpy(work->outBuffer, result, strlen(result));

    // work->output = new std::string(result);


    // work->outBuffer[0] = 'h';
    // work->outBuffer[1] = 'e';
    // work->outBuffer[2] = 'l';
    // work->outBuffer[3] = 'l';
}

void JsonTaskDone(uv_work_t *req, int code) {
  printf("json task done\n");
  Isolate *isolate = Isolate::GetCurrent();
  v8::HandleScope handleScope(isolate);

  Work *work = static_cast<Work *>(req->data);

  // v8::MaybeLocal<v8::Object> o = node::Buffer::New(isolate, (char *)work->output->c_str(), work->output->length());

  Handle<Value> argv[] = { /*o.ToLocalChecked() */ };
  Local<Function>::New(isolate, work->callback)->Call(isolate->GetCurrentContext()->Global(), 0, argv);
  work->callback.Reset();
}

// Entrypoint. Called on the main thread.
void Play(const FunctionCallbackInfo<Value>& args) {
  Isolate *isolate = args.GetIsolate();

  if (args.Length() < 3) {
    isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, "Incorrect number of arguments.")));
    return;
  }

  if (!args[0]->IsString() || !args[1]->IsString() || !args[2]->IsObject()) {
    isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, "Invalid argument format. Needs (string, string, buffer).")));
    return;
  }

  Work *work = new Work();
  work->request.data = work;

printf("starting starting starting\n");

  // Copy input strings to heap
  String::Utf8Value inputV8Str(args[0]->ToString());
  std::string *inputStr = new std::string(*inputV8Str);
  work->input = inputStr;

  String::Utf8Value tapeV8Str(args[1]->ToString());
  std::string *tapeStr = new std::string(*tapeV8Str);
  work->tape = tapeStr;

  // Save the callback
  Local<Function> callback = Local<Function>::Cast(args[3]);
  work->callback.Reset(isolate, callback);



  unsigned char *buffer = (unsigned char *) node::Buffer::Data(args[2]);

  printf("starting starting starting\n");
  work->outBuffer = buffer;



  // Local<Object> outBuffer = Local<Object>::Cast(args[2]);
  // work->outBuffer.Reset(isolate, outBuffer);
  
  uv_queue_work(uv_default_loop(), &work->request, StartJsonTask, JsonTaskDone);

  args.GetReturnValue().Set(Undefined(isolate));
}

void Init(Local<Object> exports) {
  NODE_SET_METHOD(exports, "play", Play);
}

NODE_MODULE(addon, Init)

}
