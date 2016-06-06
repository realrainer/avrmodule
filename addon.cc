// addon.cc

#define __STDC_CONSTANT_MACROS

#include <node.h>
#include "artspinput.h"
#include "ahttpinput.h"
#include "astreamdecode.h"

#include "frameblocks.h"

using namespace v8;

void DestroyAll(const FunctionCallbackInfo<Value>& args) {
    aRTSPInput::Destroy();
    aHTTPInput::Destroy();
}

void InitAll(Handle<Object> exports) {

    aRTSPInput::Init(exports);
    aHTTPInput::Init(exports);
    aStreamDecode::Init(exports);

    NODE_SET_METHOD(exports, "destroy", DestroyAll);
}

NODE_MODULE(addon, InitAll)
