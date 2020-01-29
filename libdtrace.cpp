#include <nan.h>

using namespace Nan;
using namespace v8;

class DTraceConsumer : public Nan::ObjectWrap {
public:
  static NAN_MODULE_INIT(Init);
  static NAN_METHOD(NewInstance);

private:
  explicit DTraceConsumer(double);
  ~DTraceConsumer() {}

  static NAN_METHOD(New);
  static NAN_METHOD(GetValue);

  static Nan::Persistent<v8::Function> constructor;

  double value_;
};

Nan::Persistent<v8::Function> DTraceConsumer::constructor;

DTraceConsumer::DTraceConsumer(double value = 0) { this->value_ = value; }

NAN_MODULE_INIT(DTraceConsumer::Init) {
  v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  Nan::SetPrototypeMethod(tpl, "getValue", GetValue);
  constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(DTraceConsumer::NewInstance) {
  v8::Local<v8::Function> cons = Nan::New(constructor);
  double value = info[0]->IsNumber() ? Nan::To<double>(info[0]).FromJust() : 0;
  const int argc = 1;
  v8::Local<v8::Value> argv[1] = {Nan::New(value)};
  info.GetReturnValue().Set(
      Nan::NewInstance(cons, argc, argv).ToLocalChecked());
}

NAN_METHOD(DTraceConsumer::GetValue) {
  DTraceConsumer *obj = ObjectWrap::Unwrap<DTraceConsumer>(info.Holder());
  info.GetReturnValue().Set(obj->value_);
}

NAN_METHOD(DTraceConsumer::New) {
  if (info.IsConstructCall()) {
    double value =
        info[0]->IsNumber() ? Nan::To<double>(info[0]).FromJust() : 0;
    DTraceConsumer *obj = new DTraceConsumer(value);
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    const int argc = 1;
    v8::Local<v8::Value> argv[argc] = {info[0]};
    v8::Local<v8::Function> cons = Nan::New(constructor);
    info.GetReturnValue().Set(
        Nan::NewInstance(cons, argc, argv).ToLocalChecked());
  }
}

NAN_MODULE_INIT(Init) {
  DTraceConsumer::Init(target);
  Nan::Set(target,
           Nan::New<v8::String>("newFactoryObjectInstance").ToLocalChecked(),
           Nan::GetFunction(
               Nan::New<v8::FunctionTemplate>(DTraceConsumer::NewInstance))
               .ToLocalChecked());
}

NODE_MODULE(libdtrace, Init)
