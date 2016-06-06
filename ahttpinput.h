
#ifndef __ahttpinput_h__
#define __ahttpinput_h__

#include <node.h>
#include <node_object_wrap.h>
#include <node_buffer.h>

#include <uv.h>

#include <nan.h>

#include <iostream>

#include "frameblocks.h"

extern "C" {
#include "string.h"
#include <pthread.h>
#include <unistd.h>
}

#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/multi.h>

#define URL_MAXLENGTH      256

class aHTTPInput : public node::ObjectWrap {

public:
    static void Init(v8::Handle<v8::Object> exports);
    static void Destroy();

    char url[URL_MAXLENGTH];

    cFrameBlocks frameBlocks;

    bool streamActive;

    uv_async_t async;

    int AVRCodecId;

    Nan::Callback * onDataCallBack;

    int Connect();
    int Disconnect();

    CURL* pCurl;
    bool frameStart;

private:
    explicit aHTTPInput(char *, int);
    ~aHTTPInput();

    static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void start(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void stop(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void restart(const v8::FunctionCallbackInfo<v8::Value>& args);
    static v8::Persistent<v8::Function> constructor;
};

#endif
