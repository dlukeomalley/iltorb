#include "get_params.h"
#include "stream_encode.h"
#include "stream_encode_worker.h"

using namespace v8;

StreamEncode::StreamEncode(BrotliEncoderParams params) {
  state = BrotliEncoderCreateInstance(Allocator::Alloc, Allocator::Free, &alloc);
  BrotliEncoderSetParameter(state, BROTLI_PARAM_MODE, params.mode);
  BrotliEncoderSetParameter(state, BROTLI_PARAM_QUALITY, params.quality);
  BrotliEncoderSetParameter(state, BROTLI_PARAM_LGWIN, params.lgwin);
  BrotliEncoderSetParameter(state, BROTLI_PARAM_LGBLOCK, params.lgblock);
}

StreamEncode::~StreamEncode() {
  BrotliEncoderDestroyInstance(state);
}

void StreamEncode::Init(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target) {
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("StreamEncode").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  Nan::SetPrototypeMethod(tpl, "getBlockSize", GetBlockSize);
  Nan::SetPrototypeMethod(tpl, "copy", Copy);
  Nan::SetPrototypeMethod(tpl, "encode", Encode);

  constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
  Nan::Set(target, Nan::New("StreamEncode").ToLocalChecked(),
    Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(StreamEncode::New) {
  StreamEncode* obj = new StreamEncode(getParams(info[0]->ToObject()));
  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(StreamEncode::GetBlockSize) {
  StreamEncode* obj = ObjectWrap::Unwrap<StreamEncode>(info.Holder());
  info.GetReturnValue().Set(Nan::New<Number>(BrotliEncoderInputBlockSize(obj->state)));
}

NAN_METHOD(StreamEncode::Copy) {
  StreamEncode* obj = ObjectWrap::Unwrap<StreamEncode>(info.Holder());

  Local<Object> buffer = info[0]->ToObject();
  const size_t input_size = node::Buffer::Length(buffer);
  const char* input_buffer = node::Buffer::Data(buffer);

  BrotliEncoderCopyInputToRingBuffer(obj->state, input_size, (const uint8_t*) input_buffer);
}

NAN_METHOD(StreamEncode::Encode) {
  StreamEncode* obj = ObjectWrap::Unwrap<StreamEncode>(info.Holder());

  bool is_last = info[0]->BooleanValue();
  Nan::Callback *callback = new Nan::Callback(info[1].As<Function>());
  StreamEncodeWorker *worker = new StreamEncodeWorker(callback, obj, is_last);
  if (info[2]->BooleanValue()) {
    Nan::AsyncQueueWorker(worker);
  } else {
    worker->Execute();
    worker->WorkComplete();
    worker->Destroy();
  }
}

Nan::Persistent<Function> StreamEncode::constructor;
