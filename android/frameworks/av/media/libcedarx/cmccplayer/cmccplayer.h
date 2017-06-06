/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : cmccplayer.h
* Description : mediaplayer adapter for operator
* History :
*   Author  : AL3
*   Date    : 2015/05/05
*   Comment : first version
*
*/

#ifndef CMCC_PLAYER_H
#define CMCC_PLAYER_H

#include <semaphore.h>
#include <pthread.h>
#include <media/MediaPlayerInterface.h>
#include "cdx_config.h"             //* configuration file in "LiBRARY/"


#if (CONFIG_OS_VERSION >= OPTION_OS_VERSION_ANDROID_5_0)
#include <media/IMediaHTTPService.h>
#endif

using namespace android;

enum CmccApplicationType
{
    CMCCNORMALPLAY,
    CMCCMIRACAST,
};


class CmccPlayer : public MediaPlayerInterface
{
public:
    CmccPlayer();
    virtual ~CmccPlayer();

    virtual status_t    initCheck();

    virtual status_t    setUID(uid_t nUid);

#if (CONFIG_OS_VERSION >= OPTION_OS_VERSION_ANDROID_5_0)
    virtual status_t    setDataSource(const sp<IMediaHTTPService> &httpService,
                        const char* pUrl, const KeyedVector<String8, String8>* pHeaders);
#else
    virtual status_t    setDataSource(const char* pUrl,
                        const KeyedVector<String8, String8>* pHeaders);
#endif
    virtual status_t    setDataSource(int fd, int64_t nOffset, int64_t nLength);
    virtual status_t    setDataSource(const sp<IStreamSource>& source);

#if CONFIG_OS_VERSION != OPTION_OS_VERSION_ANDROID_4_2
    //* android 4.4 use IGraphicBufferProducer instead of ISurfaceTexture in android 4.2.
    virtual status_t    setVideoSurfaceTexture(const sp<IGraphicBufferProducer>& bufferProducer);
#else
    virtual status_t    setVideoSurfaceTexture(const sp<ISurfaceTexture>& surfaceTexture);
#endif

    virtual status_t    prepare();
    virtual status_t    prepareAsync();
    virtual status_t    start();
    virtual status_t    stop();
    virtual status_t    pause();
    virtual bool        isPlaying();
    virtual status_t    seekTo(int nSeekTimeMs);
    virtual status_t    setSpeed(int nSpeed);

    virtual status_t    getCurrentPosition(int* msec);
    virtual status_t    getDuration(int* msec);
    virtual status_t    reset();
    virtual status_t    setLooping(int bLoop);

    virtual player_type playerType();   //* return AW_PLAYER

    virtual status_t    invoke(const Parcel &request, Parcel *reply);
    virtual void        setAudioSink(const sp<AudioSink>& audioSink);
    virtual status_t    setParameter(int key, const Parcel& request);
    virtual status_t    getParameter(int key, Parcel* reply);

    virtual status_t    getMetadata(const media::Metadata::Filter& ids, Parcel* records);

    virtual status_t    callbackProcess(int messageId, void* param);
    virtual status_t    mainThread();

    //* this method setSubCharset(const char* charset) is added by allwinner.
    virtual status_t    setSubCharset(const char* charset);
    virtual status_t    getSubCharset(char *charset);
    virtual status_t    setSubDelay(int nTimeMs);
    virtual int         getSubDelay();

private:
    virtual status_t    initializePlayer();
    virtual void        clearMediaInfo();

    virtual status_t    updateVideoInfo();

private:
    CmccPlayer(const CmccPlayer&);
    CmccPlayer &operator=(const CmccPlayer&);

    struct PlayerPriData *mPriData;

};


#endif  // AWPLAYER
