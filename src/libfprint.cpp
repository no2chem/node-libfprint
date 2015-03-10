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

    // timeout for the event handler
    this->handle_fp_timeout.tv_sec = 0;
    this->handle_fp_timeout.tv_usec = 100*1000;
}

fpreader::~fpreader() {

}

void fpreader::Init(Handle<Object> exports) {
    NanScope();
    Local<FunctionTemplate> tpl = NanNew<FunctionTemplate>(New);
    tpl->SetClassName(NanNew("fpreader"));
    tpl->InstanceTemplate()->SetInternalFieldCount(3);

    // Offer up the API to libfprint.ts
    NODE_SET_PROTOTYPE_METHOD(tpl, "close", close);
    NODE_SET_PROTOTYPE_METHOD(tpl, "enroll_finger", enroll_finger);
    NODE_SET_PROTOTYPE_METHOD(tpl, "stop_enroll_finger", stop_enroll_finger);
    NODE_SET_PROTOTYPE_METHOD(tpl, "identify_finger", identify_finger);
    NODE_SET_PROTOTYPE_METHOD(tpl, "stop_identify_finger", stop_identify_finger);
    NODE_SET_PROTOTYPE_METHOD(tpl, "handle_events", handle_events);
    NODE_SET_PROTOTYPE_METHOD(tpl, "update_database", update_database);

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




/****** begin functions of interest ******/
NAN_METHOD(fpreader::update_database)
{
    NanScope();

    // get the reader's handle
    fpreader* r = ObjectWrap::Unwrap<fpreader>(args.This());

    // load the array argument
    Local<Object> obj = args[0]->ToObject();
    Local<Array> props = obj->GetPropertyNames();

    //fp_print_data** user_array;

    if (r->user_array != NULL) {
        for (int i = 0; i < r->user_array_length; i++) {
            fp_print_data_free(r->user_array[i]);
        }
        free(r->user_array);
    }

    r->user_array_length = props->Length();
    r->user_array = (fp_print_data**)malloc(props->Length() * sizeof(fp_print_data*));

    // Iterate through args[0], adding each element to our list
    for(unsigned int i = 0; i < props->Length(); i++) {

        String::AsciiValue val(obj->Get(i)->ToString());

        int length = val.length();

        r->user_array[i] = fp_print_data_from_data( (unsigned char *)*val, (size_t)length);
    }

    NanReturnValue(NanTrue());
}

// really should be using a mutex to lock the reader when it's in use
int enrolling = 0;
int identifying = 0;

// // function for starting the asynchronous finger enrollment process
fpreader* test;
NAN_METHOD(fpreader::enroll_finger)
{
    NanScope();

    // get the reader's handle
    fpreader* r = ObjectWrap::Unwrap<fpreader>(args.This());

    // this should absolutely be a mutex
    if (!enrolling)
    {
        // lock the mutex
        enrolling = 1;

        // store a pointer to the callback function for later :)
        r->enroll_callback = new NanCallback(args[0].As<Function>());

        // start enrolling async!
        int start_code = fp_async_enroll_start(r->_dev, &enroll_stage_cb, r);
        if (start_code < 0) {
            // the enroll process never started... need to fail out gracefully
            // ...failing out gracefully
            const unsigned int argc = 5;
            Local<Value> fpimage = (Local<Value>) NanNull();
            Local<Value> fpdata = (Local<Value>) NanNull();
            Local<Value> argv[argc] = { NanNew(2), fpdata, fpimage, NanNew(0), NanNew(0) };
            enrolling = 0;
            r->enroll_callback->Call(argc, argv);
        }

        NanReturnValue(NanTrue());
    }
    else {
        NanReturnValue(NanFalse());
    }
}

// function to stop an asynchronous enrollment
NAN_METHOD(fpreader::stop_enroll_finger)
{
    NanScope();

    // get a pointer to the reader
    fpreader* r = ObjectWrap::Unwrap<fpreader>(args.This());

    // pointer to callback
    r->stop_enroll_callback = new NanCallback(args[0].As<Function>());

    // if enrollment is occuring, stop it
    if (enrolling) { // TODO mutex this shite 

        // stop the enrollment immediately
        int start_code = fp_async_enroll_stop(r->_dev, &enroll_stop_cb, r);
        if (start_code < 0) {

            // the enrollment failed to stop... it is presumed alive
            Local<Value> argv2[1] = {NanNew(2)};
            r->stop_enroll_callback->Call(1, argv2); // failure
        } else {

            // enrollment has stopped successfully, which means we need to trigger the 
            // enroll function's callback in failure mode
            const unsigned int argc = 5;
            Local<Value> fpimage = (Local<Value>) NanNull();
            Local<Value> fpdata = (Local<Value>) NanNull();
            Local<Value> argv[argc] = { NanNew(200), fpdata, fpimage, NanNew(0), NanNew(0) };
            enrolling = 0;
            r->enroll_callback->Call(argc, argv);

            // stop enroll callback success
            Local<Value> argv2[1] = {NanNew(1)};
            r->stop_enroll_callback->Call(1, argv2); // success
        }
    } else {
        // enrollment not running, ignore this stop command
        Local<Value> argv2[1] = {NanNew(3)};
        r->stop_enroll_callback->Call(1, argv2); // ignore
    }

    NanReturnValue(NanTrue());
}

// function to start identification
NAN_METHOD(fpreader::identify_finger)
{
    NanScope();

    // get the reader's handle
    fpreader* r = ObjectWrap::Unwrap<fpreader>(args.This());

    // this should absolutely be a mutex
    if (!identifying)
    {
        // lock the mutex
        identifying = 1;

        // ****** TODO this we be an arguement passed in...
        r->user_array = NULL; // LOAD ME FROM ARGS

        // store a pointer to the callback function for later :)
        r->identify_callback = new NanCallback(args[0].As<Function>());

        // start enrolling async!
        int start_code = fp_async_identify_start(r->_dev, r->user_array, &identify_cb, r);
        if (start_code < 0) {
            // the identify process never started... need to fail out gracefully
            // ...failing out gracefully
            const unsigned int argc = 5;
            Local<Value> fpimage = (Local<Value>) NanNull();
            Local<Value> fpindex = (Local<Value>) NanNull();
            Local<Value> argv[argc] = { NanNew(2), fpindex, fpimage, NanNew(0), NanNew(0) };
            identifying = 0;
            r->identify_callback->Call(argc, argv);
        }

        NanReturnValue(NanTrue());
    }
    else {
        NanReturnValue(NanFalse());
    }
}

// function to stop identification
NAN_METHOD(fpreader::stop_identify_finger)
{
    NanScope();

    // get a pointer to the reader
    fpreader* r = ObjectWrap::Unwrap<fpreader>(args.This());

    // pointer to callback
    r->stop_identify_callback = new NanCallback(args[0].As<Function>());

    // if enrollment is occuring, stop it
    if (identifying) { // TODO mutex this shite 

        // stop the enrollment immediately
        int start_code = fp_async_identify_stop(r->_dev, &identify_stop_cb, r);
        if (start_code < 0) {

            // the enrollment failed to stop... it is presumed alive
            Local<Value> argv2[1] = {NanNew(2)};
            r->stop_identify_callback->Call(1, argv2); // failure
        } else {

            // identification has stopped successfully, which means we need to trigger the 
            // identify function's callback in failure mode
            const unsigned int argc = 5;
            Local<Value> fpimage = (Local<Value>) NanNull();
            Local<Value> fpindex = NanNew(-1);
            Local<Value> argv[argc] = { NanNew(200), fpindex, fpimage, NanNew(0), NanNew(0) };
            identifying = 0;
            r->identify_callback->Call(argc, argv);

            // stop enroll callback success
            Local<Value> argv2[1] = {NanNew(1)};
            r->stop_identify_callback->Call(1, argv2); // success
        }
    } else {
        // identification not running, ignore this stop command
        Local<Value> argv2[1] = {NanNew(3)};
        r->stop_identify_callback->Call(1, argv2); // ignore
    }

    NanReturnValue(NanTrue());
}

// Required event driver for asynchronous activity
NAN_METHOD(fpreader::handle_events)
{
    NanScope();

    // handle events
    fpreader* r = ObjectWrap::Unwrap<fpreader>(args.This());
    fp_handle_events_timeout(&(r->handle_fp_timeout));

    NanReturnValue(NanTrue());
}

// This handles the actual enrollment callback
void fpreader::EnrollStageCallback(int result, struct fp_print_data* print, struct fp_img* img) {

    NanScope();

    // declare some variables for storage
    unsigned char* print_data;
    size_t print_data_len;
    int iheight;
    int iwidth;
    int isize;
    char* image_data;

    // get the fpreader and tell it to stop enrolling
    //fpreader* r = ObjectWrap::Unwrap<fpreader>(args.This());
    fpreader* r = this;
    fp_async_enroll_stop(r->_dev, &enroll_stop_cb, r);

    // if the result of the callback is a success, happy day
    if (result == FP_ENROLL_COMPLETE)
    {
        print_data_len = fp_print_data_get_data(print, &print_data);
    }

    // TODO we should check for an image first, not all readers support this (ours does)
    fp_img_standardize(img);
    iheight = fp_img_get_height(img);
    iwidth = fp_img_get_width(img);
    isize = iheight * iwidth;
    if (isize != 0)
    {
        image_data = new char[iwidth * isize];
        memcpy(image_data, fp_img_get_data(img), isize);
    }
    fp_img_free(img);

    // build args for the callback
    const unsigned int argc = 5;
    Local<Value> fpimage = (isize == 0) ? (Local<Value>) NanNull() : (Local<Value>) NanNewBufferHandle(image_data, isize);
    Local<Value> fpdata = (result == FP_ENROLL_COMPLETE) ? (Local<Value>) NanNewBufferHandle((char*)print_data, print_data_len) : (Local<Value>) NanNull();
    Local<Value> argv[argc] = { NanNew(result), fpdata, fpimage, NanNew(iheight), NanNew(iwidth) };

    // fire that callback off
    enrolling = 0;
    r->enroll_callback->Call(argc, argv);
}

// this handles the enrollment stop callback (hint: does not do anything)
void fpreader::EnrollStopCallback() {}

void fpreader::IdentifyCallback(int result, size_t match_offset, struct fp_img *img) {

    NanScope();

    // declare some variables for storage
    int iheight;
    int iwidth;
    int isize;
    char* image_data;

    fpreader* r = this;
    fp_async_identify_stop(r->_dev, &identify_stop_cb, r);

    // if the result of the callback is a success, happy day
    if (result == FP_VERIFY_MATCH)
    {
        // TODO do something with match_offset? convert to integer?
    }

    // TODO we should check for an image first, not all readers support this (ours does)
    fp_img_standardize(img);
    iheight = fp_img_get_height(img);
    iwidth = fp_img_get_width(img);
    isize = iheight * iwidth;
    if (isize != 0)
    {
        image_data = new char[iwidth * isize];
        memcpy(image_data, fp_img_get_data(img), isize);
    }
    fp_img_free(img);

    // build args for the callback
    const unsigned int argc = 5;
    Local<Value> fpimage = (isize == 0) ? (Local<Value>) NanNull() : (Local<Value>) NanNewBufferHandle(image_data, isize);
    Local<Value> fpindex = (result == FP_ENROLL_COMPLETE) ? NanNew(static_cast<int>(match_offset)) : NanNew(-1);
    Local<Value> argv[argc] = { NanNew(result), fpindex, fpimage, NanNew(iheight), NanNew(iwidth) };

    // fire that callback off
    identifying = 0;
    r->identify_callback->Call(argc, argv);
}

void fpreader::IdentifyStopCallback() {}

/* Placeholder callback functions for redirection (left over from old code) */
void enroll_stage_cb(struct fp_dev *dev,
                     int result,
                     struct fp_print_data *print,
                     struct fp_img *img,
                     void *user_data) {

  if(user_data) {
    ((fpreader*) user_data)->EnrollStageCallback(result, print, img);
  }
}
void enroll_stop_cb(struct fp_dev *dev,
                    void *user_data) {
  if(user_data) {
    ((fpreader*) user_data)->EnrollStopCallback();
  }
}
void identify_cb(struct fp_dev *dev,
                 int result,
                 size_t match_offset,
                 struct fp_img *img,
                 void *user_data) {
  if(user_data) {
    ((fpreader*) user_data)->IdentifyCallback(result, match_offset, img);
  }
}
void identify_stop_cb(struct fp_dev *dev,
                      void *user_data) {
  if(user_data) {
    ((fpreader*) user_data)->IdentifyStopCallback();
  }
}

/****** end functions of interest *******/



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
