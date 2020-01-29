#include <nan.h>

using namespace Nan;
using namespace v8;

class DTraceConsumer : public Nan::ObjectWrap {
 public:
  static NAN_MODULE_INIT(Init) {
    v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    Nan::SetPrototypeMethod(tpl, "getValue", GetValue);

    constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
  }

  static NAN_METHOD(NewInstance) {
    v8::Local<v8::Function> cons = Nan::New(constructor());
    double value = info[0]->IsNumber() ? Nan::To<double>(info[0]).FromJust() : 0;
    const int argc = 1;
    v8::Local<v8::Value> argv[1] = {Nan::New(value)};
    info.GetReturnValue().Set(Nan::NewInstance(cons, argc, argv).ToLocalChecked());
  }

  // Needed for the next example:
  inline double value() const {
    return value_;
  }

 private:
  explicit DTraceConsumer(double value = 0) : value_(value) {}
  ~DTraceConsumer() {}

  static NAN_METHOD(New) {
    if (info.IsConstructCall()) {
      double value = info[0]->IsNumber() ? Nan::To<double>(info[0]).FromJust() : 0;
      DTraceConsumer * obj = new DTraceConsumer(value);
      obj->Wrap(info.This());
      info.GetReturnValue().Set(info.This());
    } else {
      const int argc = 1;
      v8::Local<v8::Value> argv[argc] = {info[0]};
      v8::Local<v8::Function> cons = Nan::New(constructor());
      info.GetReturnValue().Set(Nan::NewInstance(cons, argc, argv).ToLocalChecked());
    }
  }

  static NAN_METHOD(GetValue) {
    DTraceConsumer* obj = ObjectWrap::Unwrap<DTraceConsumer>(info.Holder());
    info.GetReturnValue().Set(obj->value_);
  }

  static inline Nan::Persistent<v8::Function> & constructor() {
    static Nan::Persistent<v8::Function> my_constructor;
    return my_constructor;
  }

  double value_;
};

NAN_MODULE_INIT(Init) {
  DTraceConsumer::Init(target);
  Nan::Set(target,
    Nan::New<v8::String>("newFactoryObjectInstance").ToLocalChecked(),
    Nan::GetFunction(
      Nan::New<v8::FunctionTemplate>(DTraceConsumer::NewInstance)).ToLocalChecked()
  );
}

NODE_MODULE(libdtrace, Init)
