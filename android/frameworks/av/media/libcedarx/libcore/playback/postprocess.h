/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : postprocess.h
 * Description : post process
 * History :
 *
 */

#ifndef __POSTPROCESS__
#define __POSTPROCESS__

#include <string.h>
#include <stdio.h>

enum AW_PP_TYPE
{
    PP_TYPE_UNKNOWN,
    PP_TYPE_LINEAR_BLEND,
    PP_TYPE_LINEAR_INTERPOLATE,
    PP_TYPE_CUBIC_INTERPOLATE,
    PP_TYPE_YADIF,
};

class CPostProcess
{
public:
    CPostProcess();
    ~CPostProcess();

    bool init(AW_PP_TYPE type, int width, int height);

    void process(const uint8_t * prev[3],
                const uint8_t * src[3],
                const int srcStride[3],
                uint8_t * dst[3],
                const int dstStride[3]);

private:
    AW_PP_TYPE        mType;

    int                mWidth;
    int                mHeight;

    bool            mCheckInit;

    uint8_t            mTempDst[48*1024];
    uint8_t            mTempSrc[48*1024];
    uint8_t            mDeintTemp[4*1024];

    void doProcess(const uint8_t src[],
                const int srcStride,
                uint8_t dst[],
                const int dstStride,
                int width,
                int height);
};

#endif
