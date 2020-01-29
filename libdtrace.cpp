#include <nan.h>

#include <errno.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>

/*
 * Sadly, libelf refuses to compile if _FILE_OFFSET_BITS has been manually
 * jacked to 64 on a 32-bit compile.  In this case, we just manually set it
 * back to 32.
 */
#if defined(_ILP32) && (_FILE_OFFSET_BITS != 32)
#undef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 32
#endif

#include <dtrace.h>

using namespace Nan;
using namespace v8;

class DTraceConsumer : public Nan::ObjectWrap {
public:
  static NAN_MODULE_INIT(Init);
  static NAN_METHOD(NewInstance);

private:
  explicit DTraceConsumer();
  ~DTraceConsumer() {}

  static NAN_METHOD(New);
  static NAN_METHOD(GetValue);

  static int bufhandler(const dtrace_bufdata_t *, void *);

  static Nan::Persistent<v8::Function> constructor;
  double value_;
  dtrace_hdl_t *dtc_handle;
};

Nan::Persistent<v8::Function> DTraceConsumer::constructor;

int DTraceConsumer::bufhandler(const dtrace_bufdata_t *bufdata, void *arg) {
  return (0);
}

DTraceConsumer::DTraceConsumer() {
  int err;

  if ((this->dtc_handle = dtrace_open(DTRACE_VERSION, 0, &err)) == NULL) {
    Nan::ThrowError(dtrace_errmsg(NULL, err));
    return;
  }

  /*
   * Set our buffer size and aggregation buffer size to the de facto
   * standard of 4M.
   */

  (void)dtrace_setopt(this->dtc_handle, "bufsize", "4m");
  (void)dtrace_setopt(this->dtc_handle, "aggsize", "4m");

  if (dtrace_handle_buffered(this->dtc_handle, DTraceConsumer::bufhandler,
                             this) == -1) {
    Nan::ThrowError(dtrace_errmsg(this->dtc_handle, err));
    return;
  }

  this->value_ = 0;
}

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
    DTraceConsumer *obj = new DTraceConsumer();
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
  Nan::Set(target, Nan::New<v8::String>("DTraceConsumer").ToLocalChecked(),
           Nan::GetFunction(
               Nan::New<v8::FunctionTemplate>(DTraceConsumer::NewInstance))
               .ToLocalChecked());
}

NODE_MODULE(libdtrace, Init)
