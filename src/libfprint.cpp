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
    NODE_SET_PROTOTYPE_METHOD(tpl, "stop_enroll_finger", stop_enroll_finger);
    NODE_SET_PROTOTYPE_METHOD(tpl, "identify_finger", identify_finger);
    NODE_SET_PROTOTYPE_METHOD(tpl, "stop_identify_finger", stop_identify_finger);

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

// really should be using a mutex to lock the reader when it's in use
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

// NAN_METHOD(fpreader::enroll_finger)
// {
//     NanScope();

//     fpreader* r = ObjectWrap::Unwrap<fpreader>(args.This());
//     if (!enrolling)
//     {
//         enrolling = 1;
//         NanAsyncQueueWorker(new enroll_worker(r->_dev, new NanCallback(args[0].As<Function>())));
//         NanReturnValue(NanTrue());
//     }
//     else {
//         NanReturnValue(NanFalse());
//     }
// }

// function for starting the asynchronous finger enrollment process
fpreader* test;
NAN_METHOD(fpreader::enroll_finger)
{
    NanScope();

    // get the reader's handle
    fpreader* r = ObjectWrap::Unwrap<fpreader>(args.This());

    // TEST
    test = r;

    // this should absolutely be a mutex
    if (!enrolling)
    {
        enrolling = 1;

        // store a pointer to the callback function for later :)
        //NanAsyncQueueWorker(new enroll_worker(r->_dev, new NanCallback(args[0].As<Function>())));
        r->enroll_callback = new NanCallback(args[0].As<Function>());

        // start enrolling async!
        fp_async_enroll_start(r->_dev, &enroll_stage_cb, r);

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

    // stop the enrollment immediately
    fp_async_enroll_stop(r->_dev, &enroll_stop_cb, r);

    NanReturnValue(NanTrue());
}

NAN_METHOD(fpreader::identify_finger)
{
    NanScope();

    // test
    NanReturnValue(NanFalse());
}

NAN_METHOD(fpreader::stop_identify_finger)
{
    NanScope();

    // test
    NanReturnValue(NanFalse());
}

/////// from fpserv_async.cpp ////////
// int fpreader::StartEnroll() {
//   printf("StartEnroll()\n");
//   state = ENROLLING;
//   return fp_async_enroll_start(device, &enroll_stage_cb, this);
// }

// int fpreader::StopEnroll() {
//   state = WAITING;
//   printf("StopEnroll()\n");
//   return fp_async_enroll_stop(device, &enroll_stop_cb, this);
// }
// int fpreader::StartIdentify() {
//   printf("StartIdentify()\n");
//   state = IDENTIFYING;
//   return fp_async_identify_start(device, user_array, &identify_cb, this);
// }
// int fpreader::StopIdentify() {
//   state = WAITING;
//   printf("StopIdentify()\n");
//   return fp_async_identify_stop(device, &identify_stop_cb, this);
// }

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
void fpreader::EnrollStopCallback() {
  //TODO: assert state == WAITING
  // state = NONE;
  // printf("Enroll stopped.\n");
  // ChangeState(IDENTIFYING);
}

// void fpreader::IdentifyCallback(int result, size_t match_offset, struct fp_img *img) {
//   printf("Identify callbacked\n");
//   // If we don't immediately stop, the driver gets all excited.
//   // In our StopIdentify handler, we'll start identifying again
//   StopIdentify();

//   // Mark down that we want to continue identifying
//   next = IDENTIFYING;

//   switch(result) {
//     case FP_VERIFY_NO_MATCH:
//       // Did not find
//       SendBadRead("Fingerprint not recognized.");
//       break;
//     case FP_VERIFY_MATCH:
//       // Found it
//       SendGoodRead(users[match_offset]);
//       break;
//     case FP_VERIFY_RETRY:
//       // poor scan quality
//       SendBadRead("Fingerprint failed due to poor scan quality.");
//       break;
//     case FP_VERIFY_RETRY_TOO_SHORT:
//       // swipe too short. Not an issue with this reader.
//       SendBadRead("Fingerprint failed. Try again.");
//       break;
//     case FP_VERIFY_RETRY_CENTER_FINGER:
//       // center finger.
//       SendBadRead("Fingerprint failed. Center your finger and try again.");
//       break;
//     case FP_VERIFY_RETRY_REMOVE_FINGER:
//       // pressed too hard
//       SendBadRead("Fingerprint failed. Lift your finger and try again.");
//       break;
//     default:
//       SendBadRead("Fingerprint failed for an uknown reason.");
//       break;
//   }

//   if(img) {
//     fp_img_free(img);
//   }
// }

// void fpreader::IdentifyStopCallback() {
  
// }

/* Placeholder callback functions for redirection (left over from old code) */
void enroll_stage_cb(struct fp_dev *dev,
                     int result,
                     struct fp_print_data *print,
                     struct fp_img *img,
                     void *user_data) {

  // TEST
    const unsigned int argc = 5;
        int iheight = 0;
        int iwidth = 0;
        int newresult = 2;
        Local<Value> fpimage = (Local<Value>) NanNull();
        Local<Value> fpdata = (Local<Value>) NanNull();
        Local<Value> argv[argc] = { NanNew(newresult), fpdata, fpimage, NanNew(iheight), NanNew(iwidth) };
        enrolling = 0;
  test->enroll_callback->Call(argc, argv);
  if(user_data) {
    //((fpreader*) user_data)->EnrollStageCallback(result, print, img);

        // TEST the callback
        //const unsigned int argc = 5;
        //int iheight = 0;
        //int iwidth = 0;
        //int result = 2;
        //Local<Value> fpimage = (Local<Value>) NanNull();
        //Local<Value> fpdata = (Local<Value>) NanNull();
        //Local<Value> argv[argc] = { NanNew(result), fpdata, fpimage, NanNew(iheight), NanNew(iwidth) };
        //enrolling = 0;
        //test->enroll_callback->Call(argc, argv);
  }
}
void enroll_stop_cb(struct fp_dev *dev,
                    void *user_data) {
  if(user_data) {
    ((fpreader*) user_data)->EnrollStopCallback();
  }
}
// void identify_cb(struct fp_dev *dev,
//                  int result,
//                  size_t match_offset,
//                  struct fp_img *img,
//                  void *user_data) {
//   if(user_data) {
//     ((fpreader*) user_data)->IdentifyCallback(result, match_offset, img);
//   }
// }
// void identify_stop_cb(struct fp_dev *dev,
//                       void *user_data) {
//   if(user_data) {
//     ((fpreader*) user_data)->IdentifyStopCallback();
//   }
// }

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
