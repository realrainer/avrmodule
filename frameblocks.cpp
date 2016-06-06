
#include <stdio.h>

#include "frameblocks.h"

td_s_frameBlockInfo* cFrameBlocks::openNew(bool keyFrame) {

    td_s_frameBlockInfo * pFBI = new td_s_frameBlockInfo;
    pFBI->pBlock = malloc(blockMaxSize);

    if ((pFBI == NULL) || (pFBI->pBlock == NULL)) {
        if (pFBI != NULL) delete pFBI;
        return NULL;
    }
    pFBI->BlockSize = 0;
    pFBI->pNext = NULL;
    pFBI->keyFrame = keyFrame;
    uv_mutex_lock(&fbMutex);
    if (pFrameBlockInfoLastOpen == NULL) {
        pFrameBlockInfoLastOpen = (void*)pFBI;
        pFrameBlockInfoHead = (void*)pFBI;
    } else {
        ((td_s_frameBlockInfo*)pFrameBlockInfoLastOpen)->pNext = (void*)pFBI;
        pFrameBlockInfoLastOpen = (void*)pFBI;
    }
    uv_mutex_unlock(&fbMutex);
    return (td_s_frameBlockInfo*)pFrameBlockInfoLastOpen;
}

int cFrameBlocks::writeFrame(void *pData, int dataSize) {

    uv_mutex_lock(&fbMutex);
    if (pFrameBlockInfoLastOpen == NULL) {
        uv_mutex_unlock(&fbMutex);
        return -1;
    }

    if ((dataSize + ((td_s_frameBlockInfo*)pFrameBlockInfoLastOpen)->BlockSize) > blockMaxSize) {
        blockMaxSize = (((dataSize + ((td_s_frameBlockInfo*)pFrameBlockInfoLastOpen)->BlockSize) / FRAMEBLOCKS_BLOCK_PADDING_SIZE) + 1) * FRAMEBLOCKS_BLOCK_PADDING_SIZE;
        ((td_s_frameBlockInfo*)pFrameBlockInfoLastOpen)->pBlock = realloc(((td_s_frameBlockInfo*)pFrameBlockInfoLastOpen)->pBlock, blockMaxSize);
    }

    memcpy((((unsigned char*)((td_s_frameBlockInfo*)pFrameBlockInfoLastOpen)->pBlock) + ((td_s_frameBlockInfo*)pFrameBlockInfoLastOpen)->BlockSize), pData, dataSize);
    ((td_s_frameBlockInfo*)pFrameBlockInfoLastOpen)->BlockSize += dataSize;
    uv_mutex_unlock(&fbMutex);
    return dataSize;
}

bool cFrameBlocks::canPop() {
    td_s_frameBlockInfo* pHead = (td_s_frameBlockInfo*)pFrameBlockInfoHead;
    if (pHead == NULL) return false;
    if (pHead->pNext == NULL) return false;
    return true;
}

int cFrameBlocks::pop(td_s_frameBlockInfo &FBI) {

    td_s_frameBlockInfo* pHead = (td_s_frameBlockInfo*)pFrameBlockInfoHead;
    if (pHead == NULL) return -1;
    if (pHead->pNext == NULL) return -1;


    FBI = *pHead;
    void* pNewHead = pHead->pNext;
    delete pHead;

    pFrameBlockInfoHead = pNewHead;
    return 0;
}

cFrameBlocks::cFrameBlocks() {
    pFrameBlockInfoHead = NULL;
    pFrameBlockInfoLastOpen = NULL;
    uv_mutex_init(&fbMutex);
    blockMaxSize = FRAMEBLOCKS_BLOCK_MIN_SIZE;
}

cFrameBlocks::~cFrameBlocks() {
}
