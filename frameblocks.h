
#ifndef __frameblocks_h__
#define __frameblocks_h__

#define FRAMEBLOCKS_BLOCK_MIN_SIZE     16384
#define FRAMEBLOCKS_BLOCK_PADDING_SIZE 512

#include <uv.h>

extern "C" {
#include "string.h"
#include <stdlib.h>
}

typedef struct {
    void* pBlock;
    int BlockSize;
    bool keyFrame;
    void* pNext;
} td_s_frameBlockInfo;

class cFrameBlocks {

    void* pFrameBlockInfoHead;
    void* pFrameBlockInfoLastOpen;

    uv_mutex_t fbMutex;

    int blockMaxSize;

public:

    td_s_frameBlockInfo * openNew(bool);
    int writeFrame(void*, int);
    int pop(td_s_frameBlockInfo&);
    bool canPop();

    cFrameBlocks();
    ~cFrameBlocks();
};


#endif
