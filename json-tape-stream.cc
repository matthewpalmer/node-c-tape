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

// #include "stream_wrap.h"
// #include "stream_base.h"
// #include "stream_base-inl.h"

// #include "env-inl.h"
// #include "env.h"
// #include "handle_wrap.h"
// #include "node_buffer.h"
// #include "node_counters.h"
// #include "pipe_wrap.h"
// #include "req-wrap.h"
// #include "req-wrap-inl.h"
// #include "tcp_wrap.h"
// #include "udp_wrap.h"
// #include "util.h"
// #include "util-inl.h"

#include <stdlib.h>  // abort()
#include <string.h>  // memcpy()
#include <limits.h>  // INT_MAX


#include <typeinfo>

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
  printf("start json task\n");
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
printf("pre copy %p\n", work->outBuffer);
    // void * memcpy ( void * destination, const void * source, size_t num );
    memcpy(work->outBuffer, result, strlen(result));
printf("post tape\n");
    // work->output = new std::string(result);


    // work->outBuffer[0] = 'h';
    // work->outBuffer[1] = 'e';
    // work->outBuffer[2] = 'l';
    // work->outBuffer[3] = 'l';

}

void JsonTaskDone(uv_work_t *req, int code) {
printf("json taske done\n");
  Isolate *isolate = Isolate::GetCurrent();
  v8::HandleScope handleScope(isolate);

  Work *work = static_cast<Work *>(req->data);

  // v8::MaybeLocal<v8::Object> o = node::Buffer::New(isolate, (char *)work->output->c_str(), work->output->length());

  Handle<Value> argv[] = { /*o.ToLocalChecked() */ };
  Local<Function>::New(isolate, work->callback)->Call(isolate->GetCurrentContext()->Global(), 0, argv);
  work->callback.Reset();
}

void echo_write(uv_write_t* req, int status) {
  if (status != 0) {
    printf("%s: %s %s\n", __func__, uv_err_name(status), uv_strerror(status));
  } else {
    // uv_close((uv_handle_t*)req->handle, close_cb);
  }
  free(req);
}

// Entrypoint. Called on the main thread.
void Play(const FunctionCallbackInfo<Value>& args) {
  printf("Entry!\n");
  Isolate *isolate = args.GetIsolate();

  if (args.Length() < 3) {
    isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, "Incorrect number of arguments.")));
    return;
  }

  if (!args[0]->IsString() || !args[1]->IsString() || !args[2]->IsObject()) {
    isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, "Invalid argument format. Needs (string, string, stream).")));
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
  // Local<Function> callback = Local<Function>::Cast(args[3]);
  // work->callback.Reset(isolate, callback);

  
  // string s = typeid(YourClass).name()
  Local<Object> writeStream = Local<Object>::Cast(args[2]);
  
  uv_write_t *req = (uv_write_t *) malloc(sizeof(uv_write_t));
  uv_buf_t wrbuf = uv_buf_init("123456789\n", 10);

  // Isolate * isolate = args.GetIsolate();
  // Local<Object> target = Local<Object>::New(isolate, persist);

  // Local<String> key = String::NewFromUtf8(isolate, "x");
  // // pull the current value of prop x out of the object
  // double current = target->ToObject()->Get(key)->NumberValue();
  // // increment prop x by 42
  // target->Set(key, Number::New(isolate,  current + 42));

  Local<String> streamKey = String::NewFromUtf8(isolate, "_write");
  Local<Value> h = writeStream->ToObject()->Get(streamKey);
  Local<Function> wr = Local<Function>::Cast(h);
  printf("got a something %d\n", h->IsFunction());

  // Local<v8::Array> propertyNames = h->GetPropertyNames();
  // for (unsigned int i = 0; i < propertyNames->Length(); i++) {
  //   Local<Value> key = propertyNames->Get(i);
  //   String::Utf8Value keyUtf8(key->ToString());
  //   std::string *keyStr = new std::string(*keyUtf8);
  //   cout << *keyStr << endl;
  // }


  // uv_stream_t * stream = (uv_stream_t *)writeStream;
  // int r = uv_write(req, stream, &wrbuf, 1, echo_write);



  // unsigned char *buffer = (unsigned char *) node::Buffer::Data(args[2]);
  // work->outBuffer = buffer;

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
