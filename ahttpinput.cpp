
#define __STDC_CONSTANT_MACROS

#include "tools.h"
#include "ahttpinput.h"

static uv_loop_t* m_loop = NULL;
static uv_work_t m_req;

static char m_loopNeedRestart = 0;
static bool m_loopIsStarted = 0;

CURLM* multi_handle;
int handle_count;

static size_t curlCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    aHTTPInput* pHTTPInput = (aHTTPInput*)userp;
    int realsize = size * nmemb;
    void *p = contents;
    int bsize = realsize;

    while (bsize) {
        if (!pHTTPInput->frameStart) {
            void * pStart = memmem(p, bsize, "\xff\xd8", 2);
            if (pStart != NULL) {
                pHTTPInput->frameStart = true;
                pHTTPInput->frameBlocks.openNew(1);
                p = pStart;
                bsize = realsize - (((char*)p) - ((char*)contents));
            } else {
                bsize = 0;
            }
        }
        if (pHTTPInput->frameStart) {
            void * pEnd = memmem(p, bsize, "\xff\xd9", 2);
            if (pEnd != NULL) {
                pHTTPInput->frameStart = false;
                pHTTPInput->frameBlocks.writeFrame(p, ((char*)pEnd) - ((char*)p) + 2);
                p = (void*)(((char*)pEnd) + 2);
                bsize = realsize - (((char*)p) - ((char*)contents));
                pHTTPInput->async.data = pHTTPInput;
                uv_async_send(&pHTTPInput->async);
            } else {
                pHTTPInput->frameBlocks.writeFrame(p, realsize - (((char*)p) - ((char*)contents)));
                bsize = 0;
            }
        }
    }

    return realsize;
}

void aHTTPThread (uv_work_t *) {

    while (m_loopIsStarted) {
        m_loopNeedRestart = 0;
        while (!m_loopNeedRestart) {
            int max_fd = -1;

            fd_set readfds;
            fd_set writefds;
            fd_set excfds;
            FD_ZERO(&readfds);
            FD_ZERO(&writefds);
            FD_ZERO(&excfds);

            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000;

            curl_multi_fdset(multi_handle, &readfds, &writefds, &excfds, &max_fd);
            select(max_fd + 1, &readfds, &writefds, &excfds, &timeout);

            curl_multi_perform(multi_handle, &handle_count);
        }
    }
}

void aHTTPThreadAfter(uv_work_t *req, int status) {
    aHTTPInput* pHTTPInput = (aHTTPInput*)(req->data);
    uv_close((uv_handle_t*)&pHTTPInput->async, NULL);
}

int aHTTPInput::Connect() {
    pCurl = curl_easy_init();
    if (pCurl) {
        streamActive = true;
        curl_easy_setopt(pCurl, CURLOPT_URL, url);
        curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, curlCallback);
        curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, (void*)this);
        curl_multi_add_handle(multi_handle, pCurl);
        return 0;
    }
    return 1;
}

int aHTTPInput::Disconnect() {
    streamActive = false;
    if (pCurl) {
        curl_multi_remove_handle(multi_handle, pCurl);
        curl_easy_cleanup(pCurl);
        pCurl = NULL;
    }
    return 0;
}

using namespace v8;

void aHTTPFreeCallback(char* data, void* hint) {
    free(data);
}

void aHTTPCallBack(uv_async_t *handle) {
    aHTTPInput* pHTTPInput = (aHTTPInput*)handle->data;
    Isolate* isolate = Isolate::GetCurrent();
    Nan::HandleScope scope;

    td_s_frameBlockInfo frameBlockInfo;
    while (pHTTPInput->frameBlocks.pop(frameBlockInfo) != -1) {
        if (pHTTPInput->onDataCallBack != NULL) {
            Local<Integer> keyFrame = Integer::New(isolate, frameBlockInfo.keyFrame);
            Nan::MaybeLocal<Object> hmBuffer = Nan::NewBuffer((char*)frameBlockInfo.pBlock, frameBlockInfo.BlockSize, aHTTPFreeCallback, &pHTTPInput->frameBlocks);
            Local<Object> hBuffer;
            if (hmBuffer.ToLocal(&hBuffer)) {
                Local<Value> _args[] = {
                    hBuffer,
                    keyFrame
                };
                pHTTPInput->onDataCallBack->Call(2, _args);
            }
        } else {
            free(frameBlockInfo.pBlock);
        }
    }
}

Persistent<Function> aHTTPInput::constructor;

aHTTPInput::aHTTPInput(char * _url, int _AVRCodecId) {
    strcpy(url, _url);

    pCurl = NULL;
    frameStart = false;

    onDataCallBack = NULL;
    AVRCodecId = _AVRCodecId;
    streamActive = false;
    uv_async_init(m_loop, &async, aHTTPCallBack);
}

aHTTPInput::~aHTTPInput() {
    if (onDataCallBack != NULL) delete onDataCallBack;
    if (!uv_is_closing((uv_handle_t*)&async)) uv_close((uv_handle_t*)&async, NULL);
}

void aHTTPInput::Init(Handle<Object> exports) {

    Isolate* isolate = Isolate::GetCurrent();

    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
    tpl->SetClassName(String::NewFromUtf8(isolate, "aHTTPInput"));
    tpl->InstanceTemplate()->SetInternalFieldCount(3);

    // Prototype
    NODE_SET_PROTOTYPE_METHOD(tpl, "start", start);
    NODE_SET_PROTOTYPE_METHOD(tpl, "stop", stop);
    NODE_SET_PROTOTYPE_METHOD(tpl, "restart", restart);

    // cURL
    multi_handle = curl_multi_init();
    handle_count = 0;

    // Thread
    m_loopIsStarted = true;
    m_loop = uv_default_loop();
    uv_queue_work(m_loop, &m_req, aHTTPThread, aHTTPThreadAfter);

    constructor.Reset(isolate, tpl->GetFunction());
    exports->Set(String::NewFromUtf8(isolate, "aHTTPInput"), tpl->GetFunction());
}

void aHTTPInput::Destroy() {
    curl_multi_cleanup(multi_handle);
    m_loopIsStarted = false;
    m_loopNeedRestart = true;
}

void aHTTPInput::New(const FunctionCallbackInfo<Value>& args) {

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
        if (!strcmp(_codec, "mjpeg")) {
            _AVRCodecId = AVR_CODEC_ID_MJPEG;
        }
        if (_AVRCodecId == AVR_CODEC_ID_UNDEFINED) {
            isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Unknown codec for input")));
            return;
        }
        aHTTPInput* obj = new aHTTPInput(_url, _AVRCodecId);
        obj->Wrap(args.This());
        args.GetReturnValue().Set(args.This());

    } else {
        const int argc = 1;
        Local<Value> argv[argc] = { args[0] };
        Local<Function> cons = Local<Function>::New(isolate, constructor);
        args.GetReturnValue().Set(cons->NewInstance(argc, argv));
    }
}


void aHTTPInput::start(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    if (args.Length() < 1) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments to call")));
        return;
    }

    int result = 0;
    aHTTPInput* obj = ObjectWrap::Unwrap<aHTTPInput>(args.Holder());

    obj->onDataCallBack = new Nan::Callback(args[0].As<Function>());
    obj->Connect();

    args.GetReturnValue().Set(Number::New(isolate, result));
}

void aHTTPInput::stop(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    int result = 0;
    aHTTPInput* obj = ObjectWrap::Unwrap<aHTTPInput>(args.Holder());

    if (obj->streamActive) {
        obj->Disconnect();
    }

    args.GetReturnValue().Set(Number::New(isolate, result));
}

void aHTTPInput::restart(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    int result = 0;
    aHTTPInput* obj = ObjectWrap::Unwrap<aHTTPInput>(args.Holder());

    if (obj->streamActive) {
        obj->Disconnect();
    }
    obj->Connect();

    args.GetReturnValue().Set(Number::New(isolate, result));
}
