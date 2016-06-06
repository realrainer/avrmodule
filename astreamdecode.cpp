
#define __STDC_CONSTANT_MACROS

#include "tools.h"
#include "astreamdecode.h"

static uv_mutex_t streamDecodeInitMutex;

void aStreamDecodeThread (uv_work_t * req) {
    aStreamDecode* pStreamDecode = (aStreamDecode*)(req->data);

    AVCodec *codec = NULL;
    AVCodec *pOutCodec = NULL;
    AVCodec *pOutVideoCodec = NULL;
    AVCodecContext *c = NULL;
    AVCodecContext *pOutCtx = NULL;
    AVCodecContext *pOutVideoCtx = NULL;
    AVFrame *frame;
    AVFrame *frame_small;
    SwsContext * convert_ctx;

    AVPacket avpkt;
    AVPacket OutPacket;
    AVPacket OutVideoPacket;

    uint8_t inbuf[INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
    unsigned int inbuf_size;

    uv_mutex_lock(&streamDecodeInitMutex);

    bool previewEnable = (pStreamDecode->outputW != 0) && (pStreamDecode->outputH != 0);

    av_log_set_level(AV_LOG_INFO);

    av_register_all();
    avcodec_register_all();

    av_init_packet(&avpkt);
    memset(inbuf + INBUF_SIZE, 0, FF_INPUT_BUFFER_PADDING_SIZE);
    inbuf_size = 0;

    if (pStreamDecode->AVRCodecId == AVR_CODEC_ID_H264) {
        codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        c = avcodec_alloc_context3(codec);
        c->pix_fmt = PIX_FMT_YUV420P;
        c->profile = FF_PROFILE_H264_CONSTRAINED_BASELINE;
        if (codec->capabilities & CODEC_CAP_TRUNCATED) c->flags|= CODEC_FLAG_TRUNCATED;
    } else if (pStreamDecode->AVRCodecId == AVR_CODEC_ID_MJPEG) {
        codec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
        c = avcodec_alloc_context3(codec);
        c->pix_fmt = PIX_FMT_YUV420P;
    } else return;

    if (c == NULL) return;

    c->width = pStreamDecode->inputW;
    c->height = pStreamDecode->inputH;

    if (previewEnable) pOutCodec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    pOutVideoCodec = avcodec_find_encoder(AV_CODEC_ID_MPEG1VIDEO);

    if (previewEnable) pOutCtx = avcodec_alloc_context3(pOutCodec);
    pOutVideoCtx = avcodec_alloc_context3(pOutVideoCodec);

    if (previewEnable) {
        pOutCtx->pix_fmt = PIX_FMT_YUVJ420P;
        pOutCtx->width = pStreamDecode->outputW;
        pOutCtx->height = pStreamDecode->outputH;
        pOutCtx->time_base.den = 10;
        pOutCtx->time_base.num = 1;
        pOutCtx->bit_rate = pStreamDecode->outputW * pStreamDecode->outputH * pStreamDecode->previewQuality;
        pOutCtx->bit_rate_tolerance = pStreamDecode->outputW * pStreamDecode->outputH * pStreamDecode->previewQuality;
    }

    pOutVideoCtx->pix_fmt = PIX_FMT_YUV420P;
    pOutVideoCtx->width = pStreamDecode->inputW;
    pOutVideoCtx->height = pStreamDecode->inputH;
    pOutVideoCtx->time_base.den = 25;
    pOutVideoCtx->time_base.num = 1;
    if (pStreamDecode->AVRCodecId == AVR_CODEC_ID_MJPEG) {
        pOutVideoCtx->bit_rate = pStreamDecode->inputW * pStreamDecode->inputH * pStreamDecode->viewQuality * 2;
        pOutVideoCtx->bit_rate_tolerance = pStreamDecode->inputW * pStreamDecode->inputH * pStreamDecode->viewQuality * 2;
    } else {
        pOutVideoCtx->bit_rate = pStreamDecode->inputW * pStreamDecode->inputH * pStreamDecode->viewQuality;
        pOutVideoCtx->bit_rate_tolerance = pStreamDecode->inputW * pStreamDecode->inputH * pStreamDecode->viewQuality;
    }

    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "avcodec_open2: ERROR: Could not open input codec\n");
        return;
    }
    if (previewEnable) {
        if (avcodec_open2(pOutCtx, pOutCodec, NULL) < 0) {
            fprintf(stderr, "avcodec_open2: ERROR: Could not open output codec\n");
            return;
        }
    }

    if (avcodec_open2(pOutVideoCtx, pOutVideoCodec, NULL) < 0) {
        fprintf(stderr, "avcodec_open2: ERROR: Could not open output codec\n");
        return;
    }

    frame = av_frame_alloc();
    if (previewEnable) frame_small = av_frame_alloc();

    if ((!frame) || ((previewEnable) && (!frame_small))) {
        fprintf(stderr, "av_frame_alloc: ERROR: Could not allocate video frame\n");
        return;
    }

    if (previewEnable) {
        convert_ctx = NULL;
        frame_small->format = c->pix_fmt;
        frame_small->width = pStreamDecode->outputW;
        frame_small->height = pStreamDecode->outputH;
        av_image_alloc(frame_small->data, frame_small->linesize, frame_small->width, frame_small->height, c->pix_fmt, 32);
    }
    av_log_set_level(AV_LOG_QUIET);

    uv_mutex_unlock(&streamDecodeInitMutex);

    while (pStreamDecode->isInitComplete) {
        uv_sem_wait(&pStreamDecode->inputTrueSem);

        uv_mutex_lock(&pStreamDecode->inbufMutex);
        memcpy(inbuf, pStreamDecode->inputBuffer, pStreamDecode->inputBufferSize);
        inbuf_size = pStreamDecode->inputBufferSize;
        pStreamDecode->inputBufferSize = 0;
        uv_mutex_unlock(&pStreamDecode->inbufMutex);

        unsigned int inbufindex = 0;

        while (inbufindex < inbuf_size) {
            avpkt.size = inbuf_size - inbufindex;
            avpkt.data = &inbuf[inbufindex];
            int got_frame = 0;
            int len = avcodec_decode_video2(c, frame, &got_frame, &avpkt);
            if (len > 0) {
                inbufindex += len;
            } else {
                inbufindex = inbuf_size;
            }
            if ((got_frame) && (len >= 0)) {
                int got_output;
                if (previewEnable) {
                  convert_ctx = sws_getContext(c->width, c->height, c->pix_fmt,
                    frame_small->width, frame_small->height, PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
                  sws_scale(convert_ctx, frame->data, frame->linesize, 0,
                    c->height, frame_small->data, frame_small->linesize);
                  sws_freeContext (convert_ctx);

                  av_init_packet(&OutPacket);
                  OutPacket.data = NULL;
                  OutPacket.size = 0;
                  got_output = 0;
                  avcodec_encode_video2(pOutCtx, &OutPacket, frame_small, &got_output);
                  if ((got_output) && (OutPacket.size > 0))
                  {
                    uv_mutex_lock(&pStreamDecode->previewBufferMutex);
                    memcpy(pStreamDecode->previewBuffer, OutPacket.data, OutPacket.size);
                    pStreamDecode->previewBufferSize = OutPacket.size;
                    uv_mutex_unlock(&pStreamDecode->previewBufferMutex);
                  }
                  av_free_packet(&OutPacket);
                }

                if (pStreamDecode->isNeedVideo) {
                    // mpeg1video
                    av_init_packet(&OutVideoPacket);
                    OutVideoPacket.data = NULL;
                    OutVideoPacket.size = 0;
                    got_output = 0;
                    avcodec_encode_video2(pOutVideoCtx, &OutVideoPacket, frame, &got_output);
                    if ((got_output) && (OutVideoPacket.size > 0))
                    {
                        uv_mutex_lock(&pStreamDecode->videoBufferMutex);
                        memcpy(pStreamDecode->videoBuffer, OutVideoPacket.data, OutVideoPacket.size);
                        pStreamDecode->videoBufferSize = OutVideoPacket.size;
                        uv_mutex_unlock(&pStreamDecode->videoBufferMutex);
                    }
                    av_free_packet(&OutVideoPacket);
                }
            }
        }
        if ((pStreamDecode->previewBufferSize) || (pStreamDecode->videoBufferSize)) {
            pStreamDecode->async.data = (void*)(pStreamDecode);
            pStreamDecode->needPreviewCallBack = (pStreamDecode->previewBufferSize > 0);
            pStreamDecode->needVideoCallBack = (pStreamDecode->videoBufferSize > 0);
            uv_async_send(&pStreamDecode->async);
        }
    }

    avpkt.data = NULL;
    avpkt.size = 0;
    if (c != NULL) {
        avcodec_close(c);
        av_free(c);
    }
    if ((previewEnable) && (pOutCtx != NULL)) {
        avcodec_close(pOutCtx);
        av_free(pOutCtx);
    }
    if (pOutVideoCtx != NULL) {
        avcodec_close(pOutVideoCtx);
        av_free(pOutVideoCtx);
    }

    av_frame_free(&frame);
    if (previewEnable) av_frame_free(&frame_small);
}

void aStreamDecodeAfter(uv_work_t *req, int status) {
    aStreamDecode* pStreamDecode = (aStreamDecode*)(req->data);
    uv_close((uv_handle_t*)&pStreamDecode->async, NULL);
}

using namespace v8;

void aStreamDecodeFreeCallback(char* data, void* hint) {
    assert(data == hint);
    free(hint);
}

void aStreamDecodeCallBack(uv_async_t *handle) {
    aStreamDecode* pStreamDecode = (aStreamDecode*)handle->data;
    Nan::HandleScope scope;

    if ((pStreamDecode->needPreviewCallBack) && (pStreamDecode->onPreviewCallBack != NULL)) {
        pStreamDecode->needPreviewCallBack = false;

        uv_mutex_lock(&pStreamDecode->previewBufferMutex);
        void* buf = NULL;
        int buf_size = pStreamDecode->previewBufferSize;
        if (buf_size > 0) {
            buf = malloc(buf_size);
            if (buf != NULL) {
                memcpy(buf, pStreamDecode->previewBuffer, buf_size);
            }
            pStreamDecode->previewBufferSize = 0;
        }
        uv_mutex_unlock(&pStreamDecode->previewBufferMutex);

        if ((buf_size > 0) && (buf != NULL)) {
            Nan::MaybeLocal<Object> hmBuffer = Nan::NewBuffer((char*)buf, buf_size, aStreamDecodeFreeCallback, buf);
            Local<Object> hBuffer;
            if (hmBuffer.ToLocal(&hBuffer)) {
                Local<Value> _args[] = {
                    hBuffer
                };
                pStreamDecode->onPreviewCallBack->Call(1, _args);
            }
        }
    }
    if ((pStreamDecode->needVideoCallBack) && (pStreamDecode->onVideoCallBack != NULL)) {
        pStreamDecode->needVideoCallBack = false;

        uv_mutex_lock(&pStreamDecode->videoBufferMutex);
        void* buf2 = NULL;
        int buf2_size = pStreamDecode->videoBufferSize;
        if (buf2_size > 0) {
            buf2 = malloc(buf2_size);
            if (buf2 != NULL) {
                memcpy(buf2, pStreamDecode->videoBuffer, buf2_size);
            }
            pStreamDecode->videoBufferSize = 0;
        }
        uv_mutex_unlock(&pStreamDecode->videoBufferMutex);

        if ((buf2_size > 0) && (buf2 != NULL)) {
            Nan::MaybeLocal<Object> hmBuffer = Nan::NewBuffer((char*)buf2, buf2_size, aStreamDecodeFreeCallback, buf2);
            Local<Object> hBuffer;
            if (hmBuffer.ToLocal(&hBuffer)) {
                Local<Value> _args[] = {
                    hBuffer
                };
                pStreamDecode->onVideoCallBack->Call(1, _args);
            }
        }
    }
}

Persistent<Function> aStreamDecode::constructor;

aStreamDecode::aStreamDecode(int inW, int inH, int outW, int outH, int _AVRCodecId, int _previewQuality, int _viewQuality) {

    isInitComplete = false;
    onPreviewCallBack = NULL;
    onVideoCallBack = NULL;
    needPreviewCallBack = false;
    needVideoCallBack = false;

    inputBufferSize = 0;
    previewBufferSize = 0;
    videoBufferSize = 0;

    inputW = inW;
    inputH = inH;
    outputW = outW;
    outputH = outH;
    viewQuality = _viewQuality;
    previewQuality = _previewQuality;

    isNeedVideo = 0;
    AVRCodecId = _AVRCodecId;

    uv_sem_init(&inputTrueSem, 0);

    uv_mutex_init(&inbufMutex);
    uv_mutex_init(&previewBufferMutex);
    uv_mutex_init(&videoBufferMutex);

    isInitComplete = true;

    loop = uv_default_loop();
    req.data = (void*)this;
    uv_async_init(loop, &async, aStreamDecodeCallBack);
    uv_queue_work(loop, &req, aStreamDecodeThread, aStreamDecodeAfter);

}

aStreamDecode::~aStreamDecode() {
    __destroy();
    if (!uv_is_closing((uv_handle_t*)&async)) uv_close((uv_handle_t*)&async, NULL);
    if (onPreviewCallBack != NULL) delete onPreviewCallBack;
    if (onVideoCallBack != NULL) delete onVideoCallBack;
    uv_sem_destroy(&inputTrueSem);
}

void aStreamDecode::__destroy() {
    isInitComplete = false;
    uv_sem_post(&inputTrueSem);
}

void aStreamDecode::Init(Handle<Object> exports) {

    Isolate* isolate = Isolate::GetCurrent();

    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
    tpl->SetClassName(String::NewFromUtf8(isolate, "aStreamDecode"));
    tpl->InstanceTemplate()->SetInternalFieldCount(5);

    NODE_SET_PROTOTYPE_METHOD(tpl, "onPreview", onPreview);
    NODE_SET_PROTOTYPE_METHOD(tpl, "onVideo", onVideo);
    NODE_SET_PROTOTYPE_METHOD(tpl, "decode", decode);
    NODE_SET_PROTOTYPE_METHOD(tpl, "needVideo", needVideo);
    NODE_SET_PROTOTYPE_METHOD(tpl, "destroy", destroy);

    uv_mutex_init(&streamDecodeInitMutex);

    constructor.Reset(isolate, tpl->GetFunction());
    exports->Set(String::NewFromUtf8(isolate, "aStreamDecode"), tpl->GetFunction());
}


void aStreamDecode::New(const FunctionCallbackInfo<Value>& args) {

    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    if (args.IsConstructCall()) {
        if (args.Length() < 7) {
            isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments to call")));
            return;
        }
        if (!args[0]->IsNumber() || !args[1]->IsNumber() || !args[2]->IsNumber() || !args[3]->IsNumber() || !args[4]->IsString() || !args[5]->IsNumber() || !args[6]->IsNumber()) {
            isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong arguments")));
            return;
        }

        int _AVRCodecId = AVR_CODEC_ID_UNDEFINED;
        String::Utf8Value codecString(args[4]->ToString());
        char _codec[CODEC_MAXLENGTH];
        memcpy(_codec, *codecString, codecString.length() + 1);
        if (!strcmp(_codec, "h264")) {
            _AVRCodecId = AVR_CODEC_ID_H264;
        } else if (!strcmp(_codec, "mjpeg")) {
            _AVRCodecId = AVR_CODEC_ID_MJPEG;
        }
        if (_AVRCodecId == AVR_CODEC_ID_UNDEFINED) {
            isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Unknown codec for input")));
            return;
        }

        aStreamDecode* obj = new aStreamDecode(args[0]->NumberValue(), args[1]->NumberValue(), args[2]->NumberValue(), args[3]->NumberValue(), _AVRCodecId, args[5]->NumberValue(), args[6]->NumberValue());
        obj->Wrap(args.This());
        args.GetReturnValue().Set(args.This());

    } else {
        const int argc = 1;
        Local<Value> argv[argc] = { args[0] };
        Local<Function> cons = Local<Function>::New(isolate, constructor);
        args.GetReturnValue().Set(cons->NewInstance(argc, argv));
    }
}

void aStreamDecode::onPreview(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    if (args.Length() < 1) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments to call")));
        return;
    }
    int result = 0;

    aStreamDecode* obj = ObjectWrap::Unwrap<aStreamDecode>(args.Holder());
    if (obj->onPreviewCallBack != NULL) delete obj->onPreviewCallBack;
    obj->onPreviewCallBack = new Nan::Callback(args[0].As<Function>());
    args.GetReturnValue().Set(Number::New(isolate, result));
}

void aStreamDecode::onVideo(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    if (args.Length() < 1) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments to call")));
        return;
    }
    int result = 0;

    aStreamDecode* obj = ObjectWrap::Unwrap<aStreamDecode>(args.Holder());
    if (obj->onVideoCallBack != NULL) delete obj->onVideoCallBack;
    obj->onVideoCallBack = new Nan::Callback(args[0].As<Function>());
    args.GetReturnValue().Set(Number::New(isolate, result));
}

void aStreamDecode::decode(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    if (args.Length() < 1) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments to call")));
        return;
    }
    int result = 0;

    aStreamDecode* obj = ObjectWrap::Unwrap<aStreamDecode>(args.Holder());

    uv_mutex_lock(&obj->inbufMutex);
    Local<Object> bufferObj = args[0]->ToObject();
    if ((obj->inputBufferSize + node::Buffer::Length(bufferObj)) <= INBUF_SIZE) {
        memcpy(&obj->inputBuffer[obj->inputBufferSize], node::Buffer::Data(bufferObj), node::Buffer::Length(bufferObj));
        obj->inputBufferSize += node::Buffer::Length(bufferObj);
    }
    uv_mutex_unlock(&obj->inbufMutex);

    uv_sem_post(&obj->inputTrueSem);

    args.GetReturnValue().Set(Number::New(isolate, result));
}


void aStreamDecode::needVideo(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    if (args.Length() < 1) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments to call")));
        return;
    }
    int result = 0;
    aStreamDecode* obj = ObjectWrap::Unwrap<aStreamDecode>(args.Holder());
    obj->isNeedVideo = args[0]->NumberValue();

    args.GetReturnValue().Set(Number::New(isolate, result));
}

void aStreamDecode::destroy(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    int result = 0;

    aStreamDecode* obj = ObjectWrap::Unwrap<aStreamDecode>(args.Holder());
    obj->__destroy();

    args.GetReturnValue().Set(Number::New(isolate, result));
}
