/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : deinterlaceSw.h
 * Description : software deinterlacing
 * History :
 *
 */

#ifndef DEINTERLACE_SW_H
#define DEINTERLACE_SW_H

#include <deinterlace.h>
#include <AWPostProcess.h>

class DeinterlaceSw : public Deinterlace
{
private:
    CAWPostProcess *awPP;

public:
    DeinterlaceSw();

    ~DeinterlaceSw();

    int init();

    int reset();

    EPIXELFORMAT expectPixelFormat();

    int flag();

    int process(VideoPicture *pPrePicture,
                VideoPicture *pCurPicture,
                VideoPicture *pOutPicture,
                int nField);

};

#endif
