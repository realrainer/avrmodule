
#define __STDC_CONSTANT_MACROS

#include "tools.h"
#include "artspinput.h"
#include "artspclient.h"

static uv_loop_t* m_loop = NULL;
static uv_work_t m_req;

static char m_loopNeedRestart = 0;
static bool m_loopIsStarted = 0;

static TaskScheduler* m_scheduler = NULL;
static UsageEnvironment* m_env = NULL;

void aRTSPThread (uv_work_t *) {

    m_scheduler = BasicTaskScheduler::createNew();
    m_env = BasicUsageEnvironment::createNew(*m_scheduler);

    while (m_loopIsStarted) {
        m_loopNeedRestart = 0;
        m_env->taskScheduler().doEventLoop(&m_loopNeedRestart);
    }
    m_env->reclaim(); m_env = NULL;
    delete m_scheduler; m_scheduler = NULL;

}

void aRTSPThreadAfter(uv_work_t *req, int status) {
    aRTSPInput* pRTSPInput = (aRTSPInput*)(req->data);
    uv_close((uv_handle_t*)&pRTSPInput->async, NULL);
}

int aRTSPInput::receiveFrame(void *frameData, int frameLength) {

    if (AVRCodecId == AVR_CODEC_ID_H264) {
        char start_bytes[sizeof(h264_start_code) + 3];
        memcpy(start_bytes, h264_start_code, sizeof(h264_start_code));
        memcpy(&start_bytes[sizeof(h264_start_code)], frameData, 3);
        int vopType = h264_getVopType(start_bytes, sizeof(h264_start_code) + 3);
        if (vopType >= 0) {
            frameBlocks.openNew(vopType == 0);
        }
        frameBlocks.writeFrame((void*)h264_start_code, sizeof(h264_start_code));
        frameBlocks.writeFrame(frameData, frameLength);

        if (vopType >= 0) {
            async.data = this;
            uv_async_send(&async);
        }
        return 0;
    }
    return -1;
}

using namespace v8;

void aRTSPFreeCallback(char* data, void* hint) {
    free(data);
}

void aRTSPCallBack(uv_async_t *handle) {
    aRTSPInput* pRTSPInput = (aRTSPInput*)handle->data;
    Isolate* isolate = Isolate::GetCurrent();
    Nan::HandleScope scope;

    td_s_frameBlockInfo frameBlockInfo;
    while (pRTSPInput->frameBlocks.pop(frameBlockInfo) != -1) {
        if (pRTSPInput->onDataCallBack != NULL) {
            Local<Integer> keyFrame = Integer::New(isolate, frameBlockInfo.keyFrame);
            Nan::MaybeLocal<Object> hmBuffer = Nan::NewBuffer((char*)frameBlockInfo.pBlock, frameBlockInfo.BlockSize, aRTSPFreeCallback, &pRTSPInput->frameBlocks);
            Local<Object> hBuffer;
            if (hmBuffer.ToLocal(&hBuffer)) {
                Local<Value> _args[] = {
                    hBuffer,
                    keyFrame
                };
                pRTSPInput->onDataCallBack->Call(2, _args);
            }
        } else {
            free(frameBlockInfo.pBlock);
        }
    }
}

Persistent<Function> aRTSPInput::constructor;

aRTSPInput::aRTSPInput(char * _url, int _AVRCodecId) {
    strcpy(url, _url);
    onDataCallBack = NULL;
    AVRCodecId = _AVRCodecId;
    streamActive = false;
    uv_async_init(m_loop, &async, aRTSPCallBack);
}

aRTSPInput::~aRTSPInput() {
    if (onDataCallBack != NULL) delete onDataCallBack;
    if (!uv_is_closing((uv_handle_t*)&async)) uv_close((uv_handle_t*)&async, NULL);
}

void aRTSPInput::Init(Handle<Object> exports) {

    Isolate* isolate = Isolate::GetCurrent();

    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
    tpl->SetClassName(String::NewFromUtf8(isolate, "aRTSPInput"));
    tpl->InstanceTemplate()->SetInternalFieldCount(3);

    // Prototype
    NODE_SET_PROTOTYPE_METHOD(tpl, "start", start);
    NODE_SET_PROTOTYPE_METHOD(tpl, "stop", stop);
    NODE_SET_PROTOTYPE_METHOD(tpl, "restart", restart);

    // Thread
    m_loopIsStarted = true;
    m_loop = uv_default_loop();
    uv_queue_work(m_loop, &m_req, aRTSPThread, aRTSPThreadAfter);

    constructor.Reset(isolate, tpl->GetFunction());
    exports->Set(String::NewFromUtf8(isolate, "aRTSPInput"), tpl->GetFunction());
}

void aRTSPInput::Destroy() {
    m_loopIsStarted = false;
    m_loopNeedRestart = true;
}

void aRTSPInput::New(const FunctionCallbackInfo<Value>& args) {

    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    if (args.IsConstructCall()) {
        if (args.Length() < 2) {
            isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments to call")));
            return;
        }
        String::Utf8Value urlString(args[0]->ToString());
        if (urlString.length() >= URL_MAXLENGTH) {
            isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "URL length is too large")));
            return;
        }

        char _url[URL_MAXLENGTH];
        memcpy(_url, *urlString, urlString.length() + 1);

        int _AVRCodecId = AVR_CODEC_ID_UNDEFINED;
        String::Utf8Value codecString(args[1]->ToString());
        char _codec[CODEC_MAXLENGTH];
        memcpy(_codec, *codecString, codecString.length() + 1);
        if (!strcmp(_codec, "h264")) {
            _AVRCodecId = AVR_CODEC_ID_H264;
        }
        if (_AVRCodecId == AVR_CODEC_ID_UNDEFINED) {
            isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Unknown codec for input")));
            return;
        }
        aRTSPInput* obj = new aRTSPInput(_url, _AVRCodecId);
        obj->Wrap(args.This());
        args.GetReturnValue().Set(args.This());

    } else {
        const int argc = 1;
        Local<Value> argv[argc] = { args[0] };
        Local<Function> cons = Local<Function>::New(isolate, constructor);
        args.GetReturnValue().Set(cons->NewInstance(argc, argv));
    }
}


void aRTSPInput::start(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    if (args.Length() < 1) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments to call")));
        return;
    }

    int result = 0;
    aRTSPInput* obj = ObjectWrap::Unwrap<aRTSPInput>(args.Holder());
    obj->onDataCallBack = new Nan::Callback(args[0].As<Function>());
    obj->pRTSPClient = openURL(*m_env, "aRTSPThread", obj->url, obj);
    args.GetReturnValue().Set(Number::New(isolate, result));
}

void aRTSPInput::stop(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    int result = 0;
    aRTSPInput* obj = ObjectWrap::Unwrap<aRTSPInput>(args.Holder());

    if ((obj->pRTSPClient != NULL) && (obj->streamActive)) {
        shutdownStream(obj->pRTSPClient);
    }

    args.GetReturnValue().Set(Number::New(isolate, result));
}

void aRTSPInput::restart(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    int result = 0;
    aRTSPInput* obj = ObjectWrap::Unwrap<aRTSPInput>(args.Holder());

    if ((obj->pRTSPClient != NULL) && (obj->streamActive)) {
        shutdownStream(obj->pRTSPClient);
    }
    obj->pRTSPClient = openURL(*m_env, "aRTSPThread", obj->url, obj);

    args.GetReturnValue().Set(Number::New(isolate, result));
}
