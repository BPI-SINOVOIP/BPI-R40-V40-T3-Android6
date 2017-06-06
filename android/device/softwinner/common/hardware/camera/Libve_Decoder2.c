#define LOG_TAG    "Libev_decorder2"


#include "Libve_Decoder2.h"

#include<android/log.h>
#include <stdio.h>
#include <time.h>
#include <CameraDebug.h>
#include <utils/Log.h>
#include<stdlib.h>
#include<string.h>

//#include "vdecoder.h"


static int saveframe(char *str,void *p,int length,int is_oneframe)
{
    FILE *fd;

    if(is_oneframe){
        fd = fopen(str,"wb");
    }
    else{
        fd = fopen(str,"a");
    }

    if(!fd){
        ALOGE("Open file error");
        return -1;
    }
    if(fwrite(p,1,length,fd)){
       ALOGD("Write file successfully");
       fclose(fd);
       return 0;
    }
    else {
       ALOGE("Write file fail");
       fclose(fd);
       return -1;
    }
}

static int GetStreamData(const void* in, char*  buf0, int buf_size0, char* buf1, int buf_size1, VideoStreamDataInfo* dataInfo)
{
    ALOGD("Starting get stream data!!");
    if(dataInfo->nLength <= buf_size0) {
            ALOGV("The stream lengh is %d, the buf_size0 is %d",dataInfo->nLength,buf_size0);
            memcpy(buf0, in, dataInfo->nLength);
    }
    else {
        if(dataInfo->nLength <= (buf_size0+buf_size1)){
            ALOGV("The stream lengh is %d, the buf_size0 is %d,the buf_size1 is %d",dataInfo->nLength,buf_size0,buf_size1);
            memcpy((void *)buf0, (void *)in, buf_size0);
            memcpy(buf1,((void *)((unsigned char *)in+buf_size0)),(dataInfo->nLength -buf_size0));
        }
        else
            return -1;
    }
    dataInfo->bIsFirstPart = 1;
    dataInfo->bIsLastPart  = 1;
    dataInfo->pData       = buf0;
    dataInfo->nPts        = -1;
    dataInfo->nPcr        = -1;

    return 0;
}

void Libve_dec2(VideoDecoder** mVideoDecoder, const void *in, void *out, VideoStreamInfo* pVideoInfo, VideoStreamDataInfo* dataInfo, __attribute__((unused))VConfig* pVconfig,struct ScMemOpsS* memOpsS)
{
    int   ret;
    char* pBuf0;
    char* pBuf1;
    int size0;
    int size1;
    char* pData;

    VideoPicture*     pPicture;
    if(*mVideoDecoder == NULL)
    {
        ALOGE("mVideoDecoder = NULL, return");
        return;
    }

    ret = RequestVideoStreamBuffer(*mVideoDecoder, dataInfo->nLength, &pBuf0, &size0, &pBuf1, &size1,0);

    if(ret < 0)
    {
        ALOGE("FUNC:%s, LINE:%d, RequestVideoStreamBuffer fail!",__FUNCTION__,__LINE__);
        return;
    }

    GetStreamData(in,pBuf0,size0,pBuf1,size1,dataInfo);

    SubmitVideoStreamData(*mVideoDecoder, dataInfo, 0);

    //* decode stream.
    ret = DecodeVideoStream(*mVideoDecoder, 0 /*eos*/, 0/*key frame only*/, 0/*drop b frame*/, 0/*current time*/);

    if(ret == VDECODE_RESULT_FRAME_DECODED || ret == VDECODE_RESULT_KEYFRAME_DECODED)
    {
        pPicture = RequestPicture(*mVideoDecoder, 0/*the major stream*/);

        if(pPicture)
        {
            memOpsS->flush_cache((void*)pPicture->pData0,pVideoInfo->nWidth*pVideoInfo->nHeight);
            memOpsS->flush_cache((void*)pPicture->pData1,pVideoInfo->nWidth*pVideoInfo->nHeight/2);
            memcpy(out, (void*)pPicture->pData0, pVideoInfo->nWidth*pVideoInfo->nHeight);
            memcpy((char*)out + pVideoInfo->nWidth*pVideoInfo->nHeight, (void*)pPicture->pData1, pVideoInfo->nWidth*pVideoInfo->nHeight/2);

            ReturnPicture(*mVideoDecoder, pPicture);
        }
    }
}

int Libve_init2(VideoDecoder** mVideoDecoder, VideoStreamInfo* pVideoInfo, VConfig* pVconfig)
{
    if(*mVideoDecoder != NULL)
    {
        ALOGE("FUNC: %s fail, LINE: %d, mVideoDecoder is not NULL, please check it!",__FUNCTION__, __LINE__);
        return -1;
    }

    *mVideoDecoder = CreateVideoDecoder();

    //* initialize the decoder.
    if(InitializeVideoDecoder(*mVideoDecoder, pVideoInfo, pVconfig) != 0)
    {
        ALOGE("initialize video decoder fail.");
        DestroyVideoDecoder(*mVideoDecoder);
        *mVideoDecoder = NULL;
        return -1;
    }

    return 0;
}

int Libve_exit2(VideoDecoder** mVideoDecoder)
{
    if(*mVideoDecoder == NULL)
    {
        ALOGE("FUNC: %s, LINE: %d, mVideoDecoder == NULL",__FUNCTION__, __LINE__);
        return -1;
    }

    DestroyVideoDecoder(*mVideoDecoder);
    *mVideoDecoder = NULL;

    return 0;
}

