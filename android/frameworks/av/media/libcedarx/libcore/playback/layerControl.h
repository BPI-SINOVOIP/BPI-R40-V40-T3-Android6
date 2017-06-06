/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : layerControl.h
 * Description : layer control
 * History :
 *
 */

#ifndef LAYER_CONTROL
#define LAYER_CONTROL

#include "player_i.h"
#include "videoDecComponent.h"
#include "vdecoder.h"

typedef void* LayerCtrl;

const int MESSAGE_ID_LAYER_RETURN_BUFFER = 0x31;

const int LAYER_RESULT_USE_OUTSIDE_BUFFER = 0x2;

typedef struct NewLayerControlOpsS
{

    LayerCtrl* (*init)(void*, int);

    void (*release)(LayerCtrl*);

    int (*setDisplayBufferSize)(LayerCtrl* , int , int );

    int (*setDisplayBufferCount)(LayerCtrl* , int );

    int (*setDisplayRegion)(LayerCtrl* , int , int , int , int );

    int (*setDisplayPixelFormat)(LayerCtrl* , enum EPIXELFORMAT );

    int (*setVideoWithTwoStreamFlag)(LayerCtrl* , int );

    int (*setIsSoftDecoderFlag)(LayerCtrl* , int);

    int (*setBufferTimeStamp)(LayerCtrl* , int64_t );

    void (*resetNativeWindow)(LayerCtrl* ,void*);

    VideoPicture* (*getBufferOwnedByGpu)(LayerCtrl* );

    int (*getDisplayFPS)(LayerCtrl* );

    int (*getBufferNumHoldByGpu)(LayerCtrl* );

    int (*ctrlShowVideo)(LayerCtrl* );

    int (*ctrlHideVideo)(LayerCtrl*);

    int (*ctrlIsVideoShow)(LayerCtrl* );

    int (*ctrlHoldLastPicture)(LayerCtrl* , int );

    int (*dequeueBuffer)(LayerCtrl* , VideoPicture** , int);

    int (*queueBuffer)(LayerCtrl* , VideoPicture* , int);

    int (*releaseBuffer)(LayerCtrl* ,VideoPicture*);
}NewLayerControlOpsT;

typedef struct LayerControlOpsS
{

    LayerCtrl* (*init)(void*);

    void (*release)(LayerCtrl*);

    int (*setCallback)(LayerCtrl* , PlayerCallback , void* );

    int (*setDisplayBufferSize)(LayerCtrl* , int , int );

    int (*setDisplayRegion)(LayerCtrl* , int , int , int , int );

    int (*setDisplayPixelFormat)(LayerCtrl* , enum EPIXELFORMAT );

    int (*setDeinterlaceFlag)(LayerCtrl* ,int );

    int (*setBufferTimeStamp)(LayerCtrl* , int64_t );

    int (*getRotationAngle)(LayerCtrl* );

    int (*ctrlShowVideo)(LayerCtrl* );

    int (*ctrlHideVideo)(LayerCtrl*);

    int (*ctrlIsVideoShow)(LayerCtrl* );

    int (*ctrlHoldLastPicture)(LayerCtrl* , int );

    int (*dequeueBuffer)(LayerCtrl* , VideoPicture**);

    int (*dequeue3DBuffer)(LayerCtrl* , VideoPicture** , VideoPicture** );

    int (*queueBuffer)(LayerCtrl* , VideoPicture* , int);

    int (*queue3DBuffer)(LayerCtrl* , VideoPicture* , VideoPicture* , int);
}LayerControlOpsT;

LayerControlOpsT* __GetLayerControlOps();

NewLayerControlOpsT* __GetNewLayerControlOps();

#endif
