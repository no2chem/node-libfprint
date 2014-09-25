#ifndef NODE_LIBFPRINT
#define NODE_LIBFPRINT

#include <nan.h>
#include <libfprint/fprint.h>

class fpreader : public node::ObjectWrap {
    public:
        static void Init(v8::Handle<v8::Object> exports);
    private:
        explicit fpreader(unsigned int handle =0);
        ~fpreader();

        static NAN_METHOD(New);
        static NAN_METHOD(close);
        static NAN_GETTER(enroll_stages);
        static NAN_GETTER(supports_imaging);
        static NAN_GETTER(supports_identification);
        static NAN_GETTER(img_width);
        static NAN_GETTER(img_height);

        static v8::Persistent<v8::Function> constructor;
        fp_dev* _dev;
};

#endif
