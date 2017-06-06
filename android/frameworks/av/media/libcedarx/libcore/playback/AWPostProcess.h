/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : AWPostProcess.h
 * Description : post process, used by software deinterlacing
 * History :
 *
 */

#ifndef AW_POSTPROCESS_H
#define AW_POSTPROCESS_H

#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "vdecoder.h"
#include "messageQueue.h"

#include "postprocess.h"

#define DEBUG_STATISTIC        1
#define MAX_NUM_OF_THREAD    4

enum PP_MSG
{
    PP_MSG_START    = 1 << 0,
    PP_MSG_DONE        = 1 << 1,
    PP_MSG_EXIT        = 1 << 1,
};

class CAWPostProcess;

struct THREAD_CONTEXT
{
    CAWPostProcess *    pAWpp;
    int                    index;
};
class CAWPostProcess
{
public:
    CAWPostProcess();
    ~CAWPostProcess();

    bool Process(VideoPicture* pPrePicture, VideoPicture *pInPicture,
                    VideoPicture * pOutPicture, int nField);
    bool CheckInit(int iWidth, int iHeight);
    bool CheckWorkDone();
    void Dispose();
    char                m_propset[64];

    bool                mInitOk;

    CPostProcess *        mPP[MAX_NUM_OF_THREAD];
    pthread_t            mTid[MAX_NUM_OF_THREAD];

    MessageQueue *        mMQStart;
    MessageQueue *        mMQDone;

    VideoPicture *        mPrePicture;
    VideoPicture *        mInPicture;
    VideoPicture *        mOutPicture;
    int                    mInitWidth, mInitHeight;

    int                    mNumberOfThead;
    THREAD_CONTEXT        mThreadContext[MAX_NUM_OF_THREAD];
    bool                mPPDone[MAX_NUM_OF_THREAD];

#if DEBUG_STATISTIC
    int mFrameCnt;
    int64_t mTotalTime;
#endif
};

#endif
