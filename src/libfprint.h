#ifndef NODE_LIBFPRINT
#define NODE_LIBFPRINT

#include <nan.h>
#include <libfprint/fprint.h>

class fpreader : public node::ObjectWrap {
    public:
        static void Init(v8::Handle<v8::Object> exports);
        fp_dev* _dev;
    private:
        explicit fpreader(unsigned int handle =0);
        ~fpreader();

        static NAN_METHOD(New);
        static NAN_METHOD(close);
        static NAN_METHOD(enroll_finger);
        static NAN_GETTER(enroll_stages);
        static NAN_GETTER(supports_imaging);
        static NAN_GETTER(supports_identification);
        static NAN_GETTER(img_width);
        static NAN_GETTER(img_height);

        static v8::Persistent<v8::Function> constructor;
};

class enroll_worker : public NanAsyncWorker {
    public:
        enroll_worker(fp_dev* dev, NanCallback * callback)
        : NanAsyncWorker(callback) { _dev = dev; };
        ~enroll_worker() {};
        void Execute();
        void HandleOKCallback();
        fpreader* fp;
        fp_dev* _dev;
        unsigned char* print_data;
        size_t print_data_len;
        int iheight;
        int iwidth;
        int isize;
        int result;
        char* image_data;
};
#endif
