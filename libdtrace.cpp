#include <nan.h>

#include <errno.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>

#include <dtrace.h>

using namespace Nan;
using namespace v8;

class DTraceConsumer : public Nan::ObjectWrap {
public:
  static NAN_MODULE_INIT(Init);
  static NAN_METHOD(NewInstance);

private:
  explicit DTraceConsumer();
  ~DTraceConsumer();

  // Methods exported to nodejs
  static NAN_METHOD(New);
  static NAN_METHOD(StrCompile);
  static NAN_METHOD(Go);
  static NAN_METHOD(Consume);

  // Callback functions
  static int consume_cb(const dtrace_probedata_t *, const dtrace_recdesc_t *,
                        void *);
  static int bufhandler_cb(const dtrace_bufdata_t *, void *);

  // utility functions
  v8::Local<v8::Value> probedesc(const dtrace_probedesc_t *);
  v8::Local<v8::Value> record(const dtrace_recdesc_t *, caddr_t);
  bool valid(const dtrace_recdesc_t *);

  static Nan::Persistent<v8::Function> constructor;
  dtrace_hdl_t *dtc_handle;

  /*
   * The below list of variables are not technically properties
   * of DTraceConsumer object. They are mainly used to communicate
   * information to consume_cb(). In the future, these could be
   * removed, and instead of passing `this` as arg to consume_cb(),
   * we could pass a custom struct with all the required information
   */
  Nan::Callback dtc_callback;
  const Nan::FunctionCallbackInfo<v8::Value> *dtc_info;
  v8::Local<v8::Value> dtc_error;
};

Nan::Persistent<v8::Function> DTraceConsumer::constructor;

v8::Local<v8::Value> DTraceConsumer::probedesc(const dtrace_probedesc_t *pd) {
  Local<Object> probe = Nan::New<Object>();

  probe->Set(Nan::New<String>("provider").ToLocalChecked(),
             Nan::New<String>(pd->dtpd_provider).ToLocalChecked());
  probe->Set(Nan::New<String>("module").ToLocalChecked(),
             Nan::New<String>(pd->dtpd_mod).ToLocalChecked());
  probe->Set(Nan::New<String>("function").ToLocalChecked(),
             Nan::New<String>(pd->dtpd_func).ToLocalChecked());
  probe->Set(Nan::New<String>("name").ToLocalChecked(),
             Nan::New<String>(pd->dtpd_name).ToLocalChecked());
  return (probe);
}

v8::Local<v8::Value> DTraceConsumer::record(const dtrace_recdesc_t *rec,
                                            caddr_t addr) {

  switch (rec->dtrd_action) {
  case DTRACEACT_DIFEXPR:
    switch (rec->dtrd_size) {
    case sizeof(uint64_t):
      return Nan::New<Number>(*((int64_t *)addr));
    case sizeof(uint32_t):
      return Nan::New<Number>(*((int32_t *)addr));
    case sizeof(uint16_t):
      return Nan::New<Number>(*((int16_t *)addr));
    case sizeof(uint8_t):
      return Nan::New<Number>(*((int8_t *)addr));
    default:
      return Nan::New<String>((const char *)addr).ToLocalChecked();
    }

  case DTRACEACT_SYM:
  case DTRACEACT_MOD:
  case DTRACEACT_USYM:
  case DTRACEACT_UMOD:
  case DTRACEACT_UADDR:

    dtrace_hdl_t *dtp = this->dtc_handle;
    char buf[2048], *tick, *plus;

    buf[0] = '\0';

    if (DTRACEACT_CLASS(rec->dtrd_action) == DTRACEACT_KERNEL) {
      uint64_t pc = ((uint64_t *)addr)[0];
      dtrace_addr2str(dtp, pc, buf, sizeof(buf) - 1);
    } else {
      uint64_t pid = ((uint64_t *)addr)[0];
      uint64_t pc = ((uint64_t *)addr)[1];
      dtrace_uaddr2str(dtp, pid, pc, buf, sizeof(buf) - 1);
    }

    if (rec->dtrd_action == DTRACEACT_MOD ||
        rec->dtrd_action == DTRACEACT_UMOD) {
      /*
       * If we're looking for the module name, we'll
       * return everything to the left of the left-most
       * tick -- or "<undefined>" if there is none.
       */
      if ((tick = strchr(buf, '`')) == NULL)
        return (Nan::New<String>("<unknown>").ToLocalChecked());

      *tick = '\0';
    } else if (rec->dtrd_action == DTRACEACT_SYM ||
               rec->dtrd_action == DTRACEACT_USYM) {
      /*
       * If we're looking for the symbol name, we'll
       * return everything to the left of the right-most
       * plus sign (if there is one).
       */
      if ((plus = strrchr(buf, '+')) != NULL)
        *plus = '\0';
    }

    return (Nan::New<String>(buf).ToLocalChecked());
  }

  return Nan::New<Number>(-1);
}

bool DTraceConsumer::valid(const dtrace_recdesc_t *rec) {
  dtrace_actkind_t action = rec->dtrd_action;

  switch (action) {
  case DTRACEACT_DIFEXPR:
  case DTRACEACT_SYM:
  case DTRACEACT_MOD:
  case DTRACEACT_USYM:
  case DTRACEACT_UMOD:
  case DTRACEACT_UADDR:
    return (true);

  default:
    return (false);
  }
}

int DTraceConsumer::consume_cb(const dtrace_probedata_t *data,
                               const dtrace_recdesc_t *rec, void *arg) {

  DTraceConsumer *dtc = (DTraceConsumer *)arg;
  // XXX: Get better error handling
  // dtrace_probedesc_t *pd = data->dtpda_pdesc;
  Local<Value> datum;

  Local<v8::Value> probe = dtc->probedesc(data->dtpda_pdesc);

  if (rec == NULL) {
    Local<Value> argv[1] = {probe};
    dtc->dtc_callback.Call(dtc->dtc_info->This(), 1, argv);
    return (DTRACE_CONSUME_NEXT);
  }

  if (!dtc->valid(rec)) {
    // XXX: Once we get better error reporting, we will need this
    // char errbuf[256];

    /*
     * If this is a printf(), we'll defer to the bufhandler.
     */
    if (rec->dtrd_action == DTRACEACT_PRINTF)
      return (DTRACE_CONSUME_THIS);

    // XXX: Better error handling here. Add more context.
    dtc->dtc_error = Nan::New<String>("unsupported action").ToLocalChecked();
    return (DTRACE_CONSUME_ABORT);
  }

  Local<Object> record = Nan::New<Object>();
  record->Set(Nan::New<String>("data").ToLocalChecked(),
              dtc->record(rec, data->dtpda_data));
  Local<Value> argv[2] = {probe, record};
  dtc->dtc_callback.Call(dtc->dtc_info->This(), 2, argv);

  return (DTRACE_CONSUME_THIS);
}

int DTraceConsumer::bufhandler_cb(const dtrace_bufdata_t *bufdata, void *arg) {

  dtrace_probedata_t *data = bufdata->dtbda_probe;
  const dtrace_recdesc_t *rec = bufdata->dtbda_recdesc;
  DTraceConsumer *dtc = (DTraceConsumer *)arg;

  if (rec == NULL || rec->dtrd_action != DTRACEACT_PRINTF)
    return (DTRACE_HANDLE_OK);

  v8::Local<v8::Value> probe = dtc->probedesc(data->dtpda_pdesc);
  v8::Local<v8::Object> record = Nan::New<Object>();
  record->Set(Nan::New<String>("data").ToLocalChecked(),
              Nan::New<String>(bufdata->dtbda_buffered).ToLocalChecked());
  Local<Value> argv[2] = {probe, record};

  // XXX: Verify that dtc_callback and dtc_info are set correctly
  dtc->dtc_callback.Call(dtc->dtc_info->This(), 2, argv);

  return (DTRACE_HANDLE_OK);
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

  if (dtrace_handle_buffered(this->dtc_handle, DTraceConsumer::bufhandler_cb,
                             this) == -1) {
    Nan::ThrowError(dtrace_errmsg(this->dtc_handle, err));
    return;
  }
}

DTraceConsumer::~DTraceConsumer() { dtrace_close(this->dtc_handle); }

NAN_MODULE_INIT(DTraceConsumer::Init) {
  v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  Nan::SetPrototypeMethod(tpl, "strcompile", DTraceConsumer::StrCompile);
  Nan::SetPrototypeMethod(tpl, "go", DTraceConsumer::Go);
  Nan::SetPrototypeMethod(tpl, "consume", DTraceConsumer::Consume);

  constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(DTraceConsumer::NewInstance) {
  v8::Local<v8::Function> cons = Nan::New(constructor);
  v8::Local<v8::Value> argv[] = {};
  info.GetReturnValue().Set(Nan::NewInstance(cons, 0, argv).ToLocalChecked());
}

NAN_METHOD(DTraceConsumer::StrCompile) {
  DTraceConsumer *dtc = ObjectWrap::Unwrap<DTraceConsumer>(info.Holder());

  if (info.Length() < 1 || !info[0]->IsString()) {
    Nan::ThrowTypeError("expected string program argument");
    return;
  }

  Nan::MaybeLocal<String> prog = Nan::To<String>(info[0]);
  Nan::Utf8String uprog(prog.ToLocalChecked());

  std::string program(*uprog);

  dtrace_prog_t *dp;
  if ((dp = dtrace_program_strcompile(dtc->dtc_handle, program.c_str(),
                                      DTRACE_PROBESPEC_NAME, 0, 0, NULL)) ==
      NULL) {
    Nan::ThrowError(
        dtrace_errmsg(dtc->dtc_handle, dtrace_errno(dtc->dtc_handle)));
    return;
  }

  dtrace_proginfo_t prog_info;

  if (dtrace_program_exec(dtc->dtc_handle, dp, &prog_info) == -1) {
    Nan::ThrowError(
        dtrace_errmsg(dtc->dtc_handle, dtrace_errno(dtc->dtc_handle)));
    return;
  }

  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(DTraceConsumer::Go) {
  DTraceConsumer *dtc = ObjectWrap::Unwrap<DTraceConsumer>(info.Holder());

  if (dtrace_go(dtc->dtc_handle) == -1) {
    Nan::ThrowError(
        dtrace_errmsg(dtc->dtc_handle, dtrace_errno(dtc->dtc_handle)));
    return;
  }
  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(DTraceConsumer::Consume) {

  DTraceConsumer *dtc = ObjectWrap::Unwrap<DTraceConsumer>(info.Holder());
  dtrace_hdl_t *dtp = dtc->dtc_handle;
  dtrace_workstatus_t status;

  if (info.Length() < 1 || !info[0]->IsFunction()) {
    Nan::ThrowTypeError("expected string program argument");
    return;
  }

  dtc->dtc_callback.SetFunction(info[0].As<Function>());
  dtc->dtc_info = &info;
  dtc->dtc_error = Nan::Undefined();

  status = dtrace_work(dtp, NULL, NULL, DTraceConsumer::consume_cb, dtc);

  /*
   * XXX: Get error handling work better here
       if (status == -1 && !dtc->dtc_error->IsUndefined())
           return (dtc->dtc_error);
  */

  info.GetReturnValue().Set(dtc->dtc_error);
}

NAN_METHOD(DTraceConsumer::New) {
  if (info.IsConstructCall()) {
    DTraceConsumer *obj = new DTraceConsumer();
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    v8::Local<v8::Value> argv[] = {};
    v8::Local<v8::Function> cons = Nan::New(constructor);
    info.GetReturnValue().Set(Nan::NewInstance(cons, 0, argv).ToLocalChecked());
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
