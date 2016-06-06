
#ifndef __artspinput_h__
#define __artspinput_h__

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

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

#define URL_MAXLENGTH      256
#define INPUT_BUFFER_SIZE  1000000

class aRTSPInput : public node::ObjectWrap {

public:
    static void Init(v8::Handle<v8::Object> exports);
    static void Destroy();

    char url[URL_MAXLENGTH];

    cFrameBlocks frameBlocks;

    RTSPClient * pRTSPClient;
    bool streamActive;

    uv_async_t async;

    int AVRCodecId;

    Nan::Callback * onDataCallBack;

    int receiveFrame(void *, int);

private:
    explicit aRTSPInput(char *, int);
    ~aRTSPInput();

    static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void start(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void stop(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void restart(const v8::FunctionCallbackInfo<v8::Value>& args);
    static v8::Persistent<v8::Function> constructor;
};

#endif
