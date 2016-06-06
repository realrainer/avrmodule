
#ifndef __astreamdecode_h__
#define __astreamdecode_h__

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
#include <math.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
}

#define INBUF_SIZE   3000000
#define PREVIEWBUFFER_SIZE   500000
#define VIDEOBUFFER_SIZE   500000

class aStreamDecode : public node::ObjectWrap {

public:
    static void Init(v8::Handle<v8::Object> exports);

    int inputW;
    int inputH;
    int outputW;
    int outputH;
    int viewQuality;
    int previewQuality;

    uint8_t inputBuffer[INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
    unsigned int inputBufferSize;
    uv_mutex_t inbufMutex;

    uint8_t previewBuffer[PREVIEWBUFFER_SIZE];
    unsigned int previewBufferSize;
    uv_mutex_t previewBufferMutex;

    uint8_t videoBuffer[VIDEOBUFFER_SIZE];
    unsigned int videoBufferSize;
    uv_mutex_t videoBufferMutex;

    bool isInitComplete;

    bool isNeedVideo;

    uv_sem_t inputTrueSem;

    uv_loop_t *loop;
    uv_async_t async;
    uv_work_t req;

    bool needPreviewCallBack;
    bool needVideoCallBack;
    Nan::Callback * onPreviewCallBack;
    Nan::Callback * onVideoCallBack;

    int AVRCodecId;

private:

    explicit aStreamDecode(int, int, int, int, int, int, int);
    ~aStreamDecode();
    void __destroy();

    static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void onPreview(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void onVideo(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void decode(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void needVideo(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void destroy(const v8::FunctionCallbackInfo<v8::Value>& args);
    static v8::Persistent<v8::Function> constructor;
};


#endif
