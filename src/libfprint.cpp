#include "libfprint.h"

using namespace v8;

Persistent<Function> fpreader::constructor;

static struct fp_dscv_dev ** devices = NULL;
static unsigned int count;

NAN_METHOD(init)
{
    NanScope();
    NanReturnValue(NanNew(fp_init()));
}

NAN_METHOD(exit)
{
    NanScope();
    if (devices != NULL)
    {
        fp_dscv_devs_free(devices);
        devices = NULL;
    }
    fp_exit();
    NanReturnUndefined();
}

NAN_METHOD(discover)
{
    NanScope();

    Local<Function> cb = args[0].As<Function>();

    if (devices != NULL)
    {
        //free the list of devices before rediscovery
        fp_dscv_devs_free(devices);
    }
    devices = fp_discover_devs();
    if (devices == NULL)
    {
        //failure
        NanReturnUndefined();
    }
    fp_dscv_dev** curdev = devices;
    const unsigned int argc = 5; //idx, type, driver id, driver name, driver fullname
    count = 0;
    while (*curdev != NULL)
    {
        fp_driver* curdrv = fp_dscv_dev_get_driver(*curdev);
        Local<Value> argv[argc] = { NanNew(count), NanNew(fp_dscv_dev_get_devtype(*curdev)), NanNew(fp_driver_get_driver_id(curdrv)), NanNew(fp_driver_get_name(curdrv)), NanNew(fp_driver_get_full_name(curdrv)) };
        NanMakeCallback(NanGetCurrentContext()->Global(), cb, argc, argv);
        count++;
        curdev++;
    }

    NanReturnNull();
}

fpreader::fpreader(unsigned int handle)  {
    this->_dev = fp_dev_open(devices[handle]);
}

fpreader::~fpreader() {

}

void fpreader::Init(Handle<Object> exports) {
    NanScope();
    Local<FunctionTemplate> tpl = NanNew<FunctionTemplate>(New);
    tpl->SetClassName(NanNew("fpreader"));
    tpl->InstanceTemplate()->SetInternalFieldCount(3);

    NODE_SET_PROTOTYPE_METHOD(tpl, "close", close);
    NODE_SET_PROTOTYPE_METHOD(tpl, "enroll_finger", enroll_finger);
    tpl->PrototypeTemplate()->SetAccessor(NanNew("enroll_stages"), fpreader::enroll_stages);
    tpl->PrototypeTemplate()->SetAccessor(NanNew("supports_imaging"), fpreader::supports_imaging);
    tpl->PrototypeTemplate()->SetAccessor(NanNew("supports_identification"), fpreader::supports_identification);
    tpl->PrototypeTemplate()->SetAccessor(NanNew("img_width"), fpreader::img_width);
    tpl->PrototypeTemplate()->SetAccessor(NanNew("img_height"), fpreader::img_height);

    NanAssignPersistent(constructor, tpl->GetFunction());
    exports->Set(NanNew("fpreader"), tpl->GetFunction());
}

NAN_GETTER(fpreader::enroll_stages)
{
    NanScope();

    fpreader* r = ObjectWrap::Unwrap<fpreader>(args.This());
    NanReturnValue(NanNew(fp_dev_get_nr_enroll_stages(r->_dev)));
}

NAN_GETTER(fpreader::supports_imaging)
{
    NanScope();
    fpreader* r = ObjectWrap::Unwrap<fpreader>(args.This());
    NanReturnValue(NanNew(fp_dev_supports_imaging(r->_dev) == 1 ? true: false));
}

NAN_GETTER(fpreader::supports_identification)
{
    NanScope();

    fpreader* r = ObjectWrap::Unwrap<fpreader>(args.This());
    NanReturnValue(NanNew(fp_dev_supports_identification(r->_dev) == 1 ? true : false));
}

NAN_GETTER(fpreader::img_width)
{
    NanScope();

    fpreader* r = ObjectWrap::Unwrap<fpreader>(args.This());
    NanReturnValue(NanNew(fp_dev_get_img_width(r->_dev)));
}

NAN_GETTER(fpreader::img_height)
{
    NanScope();

    fpreader* r = ObjectWrap::Unwrap<fpreader>(args.This());
    NanReturnValue(NanNew(fp_dev_get_img_height(r->_dev)));
}

int enrolling = 0;

void enroll_worker::Execute()
{
    struct fp_print_data* print;
    struct fp_img* image;
    result = fp_enroll_finger_img(_dev, &print, &image);

    if (result == FP_ENROLL_COMPLETE)
    {
        print_data_len = fp_print_data_get_data(print, &print_data);
    }

    fp_img_standardize(image);
    iheight = fp_img_get_height(image);
    iwidth = fp_img_get_width(image);
    isize = iheight * iwidth;


    if (isize != 0)
    {
        image_data = new char[iwidth * isize];
        memcpy(image_data, fp_img_get_data(image), isize);
    }
    fp_img_free(image);
}

void enroll_worker::HandleOKCallback()
{
    NanScope();

    const unsigned int argc = 5;
    Local<Value> fpimage = (isize == 0) ? (Local<Value>) NanNull() : (Local<Value>) NanNewBufferHandle(image_data, isize);
    Local<Value> fpdata = (result == FP_ENROLL_COMPLETE) ? (Local<Value>) NanNewBufferHandle((char*)print_data, print_data_len) : (Local<Value>) NanNull();
    Local<Value> argv[argc] = { NanNew(result), fpdata, fpimage, NanNew(iheight), NanNew(iwidth) };

    enrolling = 0;
    callback->Call(argc, argv);
}

NAN_METHOD(fpreader::enroll_finger)
{
    NanScope();

    fpreader* r = ObjectWrap::Unwrap<fpreader>(args.This());
    if (!enrolling)
    {
        enrolling = 1;
        NanAsyncQueueWorker(new enroll_worker(r->_dev, new NanCallback(args[0].As<Function>())));
        NanReturnValue(NanTrue());
    }
    else {
        NanReturnValue(NanFalse());
    }
}


NAN_METHOD(fpreader::New)
{
    NanScope();

    if (args.IsConstructCall())
    {
        unsigned int handle;
        if (args[0]->IsUndefined()) {NanReturnNull();}
        handle = args[0]->NumberValue();
        if (handle > count) { NanReturnNull(); } //invalid handle
        fpreader* r = new fpreader(handle);
        r->Wrap(args.This());
        enrolling = 0;
        NanReturnValue(args.This());
    }
    else {
        const int argc = 1;
        Local<Value> argv[argc] = {args[0]};
        Local<Function> cons = NanNew<Function>(constructor);
        NanReturnValue(cons->NewInstance(argc, argv));
    }
}

NAN_METHOD(fpreader::close) {
    NanScope();

    fpreader* r = ObjectWrap::Unwrap<fpreader>(args.Holder());
    fp_dev_close(r->_dev);
    NanReturnUndefined();
}

void InitAll(Handle<Object> exports) {
    exports->Set(NanNew("init"),
            NanNew<FunctionTemplate>(init)->GetFunction());

    exports->Set(NanNew("discover"),
            NanNew<FunctionTemplate>(discover)->GetFunction());

    exports->Set(NanNew("exit"),
            NanNew<FunctionTemplate>(exit)->GetFunction());

    fpreader::Init(exports);
}

NODE_MODULE(fprint, InitAll);
