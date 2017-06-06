/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : awplayer.cpp
 *
 * Description : awplayer interface implementation
 * History :
 *
 */

#include "log.h"
#include "awplayer.h"
#include "subtitleUtils.h"
#include "awStreamingSource.h"
#include "awPlayReadyLicense.h"
#include <sc_interface.h>
#include <memoryAdapter.h>
//#include <AwPluginManager.h>
#include <string.h>

#include <media/Metadata.h>
#include <media/mediaplayer.h>
#include <binder/IPCThreadState.h>
#include <media/IAudioFlinger.h>
#include <fcntl.h>

#include <version.h>
#include <cutils/properties.h> // for property_set

#include "player.h"             //* player library in "LIBRARY/PLAYER/"
#include "mediaInfo.h"
#include "demuxComponent.h"
#include "awMessageQueue.h"

#if(MUTE_DRM_WHEN_HDMI_FLAG)
#include <HDMIListerner.h>
#endif

#define FILE_AWPLAYER_CPP   //* use configuration in a awplayerConfig.h
#include "awplayerConfig.h"

#if CONFIG_OS_VERSION != OPTION_OS_VERSION_ANDROID_4_2
    //* android 4.4 use IGraphicBufferProducer instead of ISurfaceTexture in android 4.2.
    #include <gui/IGraphicBufferProducer.h>
    #include <gui/Surface.h>
#endif

//* player status.
static const int AWPLAYER_STATUS_IDLE        = 0;
static const int AWPLAYER_STATUS_INITIALIZED = 1<<0;
static const int AWPLAYER_STATUS_PREPARING   = 1<<1;
static const int AWPLAYER_STATUS_PREPARED    = 1<<2;
static const int AWPLAYER_STATUS_STARTED     = 1<<3;
static const int AWPLAYER_STATUS_PAUSED      = 1<<4;
static const int AWPLAYER_STATUS_STOPPED     = 1<<5;
static const int AWPLAYER_STATUS_COMPLETE    = 1<<6;
static const int AWPLAYER_STATUS_ERROR       = 1<<7;

//* callback message id.
static const int AWPLAYER_MESSAGE_DEMUX_PREPARED                = 0x101;
static const int AWPLAYER_MESSAGE_DEMUX_EOS                     = 0x102;
static const int AWPLAYER_MESSAGE_DEMUX_IOERROR                 = 0x103;
static const int AWPLAYER_MESSAGE_DEMUX_SEEK_FINISH             = 0x104;
static const int AWPLAYER_MESSAGE_DEMUX_CACHE_REPORT            = 0x105;
static const int AWPLAYER_MESSAGE_DEMUX_BUFFER_START            = 0x106;
static const int AWPLAYER_MESSAGE_DEMUX_BUFFER_END              = 0x107;
static const int AWPLAYER_MESSAGE_DEMUX_PAUSE_PLAYER            = 0x108;
static const int AWPLAYER_MESSAGE_DEMUX_RESUME_PLAYER           = 0x109;
static const int AWPLAYER_MESSAGE_DEMUX_IOREQ_ACCESS            = 0x10a;
static const int AWPLAYER_MESSAGE_DEMUX_IOREQ_OPEN              = 0x10b;
static const int AWPLAYER_MESSAGE_DEMUX_IOREQ_OPENDIR           = 0x10c;
static const int AWPLAYER_MESSAGE_DEMUX_IOREQ_READDIR           = 0x10d;
static const int AWPLAYER_MESSAGE_DEMUX_IOREQ_CLOSEDIR          = 0x10e;
static const int AWPLAYER_MESSAGE_DEMUX_VIDEO_STREAM_CHANGE        = 0x10f;
static const int AWPLAYER_MESSAGE_DEMUX_AUDIO_STREAM_CHANGE        = 0x110;

static const int AWPLAYER_MESSAGE_PLAYER_EOS                    = 0x201;
static const int AWPLAYER_MESSAGE_PLAYER_FIRST_PICTURE          = 0x202;
static const int AWPLAYER_MESSAGE_PLAYER_SUBTITLE_AVAILABLE     = 0x203;
static const int AWPLAYER_MESSAGE_PLAYER_SUBTITLE_EXPIRED       = 0x204;
static const int AWPLAYER_MESSAGE_PLAYER_VIDEO_SIZE             = 0x205;
static const int AWPLAYER_MESSAGE_PLAYER_VIDEO_CROP             = 0x206;
static const int AWPLAYER_MESSAGE_PLAYER_VIDEO_UNSUPPORTED      = 0x207;
static const int AWPLAYER_MESSAGE_PLAYER_AUDIO_UNSUPPORTED      = 0x208;
static const int AWPLAYER_MESSAGE_PLAYER_SUBTITLE_UNSUPPORTED   = 0x209;
static const int AWPLAYER_MESSAGE_PLAYER_AUDIORAWPLAY           = 0x20a;
static const int AWPLAYER_MESSAGE_PLAYER_SET_SECURE_BUFFER_COUNT= 0x20b;
static const int AWPLAYER_MESSAGE_PLAYER_SET_SECURE_BUFFERS     = 0x20c;
static const int AWPLAYER_MESSAGE_PLAYER_SET_AUDIO_INFO         = 0x20d;
static const int AWPLAYER_MESSAGE_PLAYER_DOWNLOAD_START         = 0x20e;
static const int AWPLAYER_MESSAGE_PLAYER_DOWNLOAD_END             = 0x20f;
static const int AWPLAYER_MESSAGE_PLAYER_DOWNLOAD_ERROR         = 0x210;

//* command id.
static const int AWPLAYER_COMMAND_SET_SOURCE    = 0x101;
static const int AWPLAYER_COMMAND_SET_SURFACE   = 0x102;
static const int AWPLAYER_COMMAND_SET_AUDIOSINK = 0x103;
static const int AWPLAYER_COMMAND_PREPARE       = 0x104;
static const int AWPLAYER_COMMAND_START         = 0x105;
static const int AWPLAYER_COMMAND_STOP          = 0x106;
static const int AWPLAYER_COMMAND_PAUSE         = 0x107;
static const int AWPLAYER_COMMAND_RESET         = 0x108;
static const int AWPLAYER_COMMAND_QUIT          = 0x109;
static const int AWPLAYER_COMMAND_SEEK          = 0x10a;

//* AliYUNOS command id.
static const int YUNOS_SOURCE                    = 0x1f000000;
static const int YUNOS_TS_INFO                    = 0x1f000001;
static const int YUNOS_HTTP_DOWNLOAD_ERROR_INFO    = 0x1f000002;
static const int YUNOS_MEDIA_INFO                = 0x1f000003;

typedef struct PlayerPriData{
        AwMessageQueue*     mMessageQueue;
        Player*             mPlayer;
        DemuxComp*          mDemux;
        pthread_t           mThreadId;
        int                 mThreadCreated;
        uid_t               mUID;             //* no use.

        //* data source.
        char*               mSourceUrl;       //* file path or network stream url.
        CdxStreamT*         mSourceStream;    //* outside streaming source like miracast.

        int                 mSourceFd;        //* file descriptor.
        int64_t             mSourceFdOffset;
        int64_t             mSourceFdLength;

        //* media information.
        MediaInfo*          mMediaInfo;

        //* note whether the sutitle is in text or in bitmap format.
        int                 mIsSubtitleInTextFormat;

        //* text codec format of the subtitle, used to transform subtitle text to
        //* utf8 when the subtitle text codec format is unknown.
        char                mDefaultTextFormat[32];

        //* whether enable subtitle show.
        int                 mIsSubtitleDisable;

        //* file descriptor of .idx file of index+sub subtitle.
        //* we save the .idx file's fd here because application set .idx file and .sub file
        //* seperately, we need to wait for the .sub file's fd, see
        //* INVOKE_ID_ADD_EXTERNAL_SOURCE_FD command in invoke() method.
        int                 mIndexFileHasBeenSet;
        int                 mIndexFileFdOfIndexSubtitle;

        //* surface.
#if CONFIG_OS_VERSION != OPTION_OS_VERSION_ANDROID_4_2
        //* android 4.4 use IGraphicBufferProducer instead of ISurfaceTexture in android 4.2.
        sp<IGraphicBufferProducer> mGraphicBufferProducer;
#else
        sp<ISurfaceTexture> mSurfaceTexture;
#endif
        sp<ANativeWindow>   mNativeWindow;

        //* for status and synchronize control.
        int                 mStatus;

        //* for media info protection.
        pthread_mutex_t     mMutexMediaInfo;

        /* for mStatus protection in start/stop/pause operation and
         * complete/seek finish callback.
         */
        pthread_mutex_t     mMutexStatus;
        sem_t               mSemSetDataSource;
        sem_t               mSemPrepare;
        sem_t               mSemStart;
        sem_t               mSemStop;
        sem_t               mSemPause;
        sem_t               mSemQuit;
        sem_t               mSemReset;
        sem_t               mSemSeek;
        sem_t               mSemSetSurface;
        sem_t               mSemSetAudioSink;

        /* for signal prepare finish, used in prepare(). */
        sem_t               mSemPrepareFinish;

        //* status control.
        int                 mSetDataSourceReply;
        int                 mPrepareReply;
        int                 mStartReply;
        int                 mStopReply;
        int                 mPauseReply;
        int                 mResetReply;
        int                 mSeekReply;
        int                 mSetSurfaceReply;
        int                 mSetAudioSinkReply;
        int                 mPrepareFinishResult;   //* save the prepare result for prepare().

        int                 mPrepareSync;   //* synchroized prarare() call, don't call back to user.
        int                 mSeeking;

        /* use to check whether seek callback is for current seek operation or
         * previous.
         */
        int                 mSeekTime;
        /* internal seek, don't call back to user. */
        int                 mSeekSync;
        int                 mLoop;
        int                 mKeepLastFrame;

        /* use to record videoSize which had send to app */
        int                 mVideoSizeWidth;
        int                 mVideoSizeHeight;

        enum ApplicationType mApplicationType;
        char                strApkName[128];

#if (CONFIG_OS_VERSION >= OPTION_OS_VERSION_ANDROID_5_0)
        sp<IMediaHTTPService> mHTTPService;
#endif

        //* record the id of subtitle which is displaying
        //* we set the Nums to 64 .(32 may be not enough)
        unsigned int        mSubtitleDisplayIds[64];
        int                 mSubtitleDisplayIdsUpdateIndex;

        //* save the currentSelectTrackIndex;
        int                 mCurrentSelectTrackIndex;
        int                 mRawOccupyFlag;

#if(MUTE_DRM_WHEN_HDMI_FLAG)
        HDMIListerner* mHDMIListener;
        bool                mMuteDRMWhenHDMI;
#endif
        //*other
        int                 mCallbackVideoSizeInStartStatusFlag;
        int                 mScaledownFlag;


        int                 mYunOSInfoEndale;
        char                mYunOSUUID[640];
        int                 mYunOSErrorCode;
        int                 mUartLogLevel;
}PlayerPrivateData;


static void* AwPlayerThread(void* arg);
static int DemuxCallbackProcess(void* pUserData, int eMessageId, void* param);
static int PlayerCallbackProcess(void* pUserData, int eMessageId, void* param);
static int GetCallingApkName(char* strApkName, int nMaxNameSize);


void enableMediaBoost(MediaInfo* mi, int* logLevelOut);
void disableMediaBoost(int logLevel);
void setMemoryLimit(Player *player);

AwPlayer::AwPlayer()
{
    logv("awplayer construct.");
//    AwPluginInit();
// TODO: plugin will add in ini config.
    LogVersionInfo();

    mPriData = (PlayerPriData*)malloc(sizeof(PlayerPriData));
    memset(mPriData,0x00,sizeof(PlayerPriData));

    mPriData->mUID            = -1;
    mPriData->mSourceUrl      = NULL;
    mPriData->mSourceFd       = -1;
    mPriData->mSourceFdOffset = 0;
    mPriData->mSourceFdLength = 0;
    mPriData->mSourceStream   = NULL;
#if CONFIG_OS_VERSION != OPTION_OS_VERSION_ANDROID_4_2
    //* android 4.4 use IGraphicBufferProducer instead of ISurfaceTexture in android 4.2.
    mPriData->mGraphicBufferProducer = NULL;
#else
    mPriData->mSurfaceTexture = NULL;
#endif
    mPriData->mNativeWindow   = NULL;
    mPriData->mStatus         = AWPLAYER_STATUS_IDLE;
    mPriData->mSeeking        = 0;
    mPriData->mSeekSync       = 0;
    mPriData->mLoop           = 0;
    mPriData->mKeepLastFrame  = 0;
    mPriData->mMediaInfo      = NULL;
    mPriData->mMessageQueue   = NULL;
    mPriData->mVideoSizeWidth = 0;
    mPriData->mVideoSizeHeight= 0;
    mPriData->mScaledownFlag  = 0;
    mPriData->mCurrentSelectTrackIndex = -1;

    pthread_mutex_init(&mPriData->mMutexMediaInfo, NULL);
    pthread_mutex_init(&mPriData->mMutexStatus, NULL);
    sem_init(&mPriData->mSemSetDataSource, 0, 0);
    sem_init(&mPriData->mSemPrepare, 0, 0);
    sem_init(&mPriData->mSemStart, 0, 0);
    sem_init(&mPriData->mSemStop, 0, 0);
    sem_init(&mPriData->mSemPause, 0, 0);
    sem_init(&mPriData->mSemReset, 0, 0);
    sem_init(&mPriData->mSemQuit, 0, 0);
    sem_init(&mPriData->mSemSeek, 0, 0);
    sem_init(&mPriData->mSemSetSurface, 0, 0);
    sem_init(&mPriData->mSemSetAudioSink, 0, 0);

    sem_init(&mPriData->mSemPrepareFinish, 0, 0); //* for signal prepare finish, used in prepare().

    mPriData->mMessageQueue = AwMessageQueueCreate(64);
    mPriData->mPlayer       = PlayerCreate();
    mPriData->mDemux        = DemuxCompCreate();

    if(mPriData->mPlayer != NULL)
        PlayerSetCallback(mPriData->mPlayer, PlayerCallbackProcess, (void*)this);

    if(mPriData->mDemux != NULL)
    {
        DemuxCompSetCallback(mPriData->mDemux, DemuxCallbackProcess, (void*)this);
        DemuxCompSetPlayer(mPriData->mDemux, mPriData->mPlayer);
    }

    if(pthread_create(&mPriData->mThreadId, NULL, AwPlayerThread, this) == 0)
        mPriData->mThreadCreated = 1;
    else
        mPriData->mThreadCreated = 0;

    strcpy(mPriData->mDefaultTextFormat, "GBK");
    mPriData->mIsSubtitleDisable = 1;
    mPriData->mIndexFileFdOfIndexSubtitle = -1;
    mPriData->mIndexFileHasBeenSet = 0;
    memset(mPriData->mSubtitleDisplayIds,0xff,64*sizeof(unsigned int));
    mPriData->mSubtitleDisplayIdsUpdateIndex = 0;
    mPriData->mApplicationType = NORMALPLAY;
    mPriData->mRawOccupyFlag = 0;
    mPriData->mCallbackVideoSizeInStartStatusFlag = 0;
    #if(MUTE_DRM_WHEN_HDMI_FLAG)
    mPriData->mMuteDRMWhenHDMI = false;
    mPriData->mHDMIListener  = new HDMIListerner();

    char value[256];
    if (property_get("ro.sys.mutedrm", value, NULL)
        && (!strncasecmp(value, "true", 4))) {
        mPriData->mMuteDRMWhenHDMI = true;
    } else {
        mPriData->mMuteDRMWhenHDMI = false;
    }
    #endif

    GetCallingApkName(mPriData->strApkName, sizeof(mPriData->strApkName));
    mPriData->mYunOSInfoEndale = 0;
    memset(mPriData->mYunOSUUID,0x00,sizeof(mPriData->mYunOSUUID));
}


AwPlayer::~AwPlayer()
{
    AwMessage msg;
    int param_occupy[3]  = {1,0,0};
    int param_release[3] = {0,0,0};
    logw("~AwPlayer");

    if(mPriData->mThreadCreated)
    {
        void* status;

        reset();    //* stop demux and player.

        //* send a quit message to quit the main thread.
        setMessage(&msg, AWPLAYER_COMMAND_QUIT, (uintptr_t)&mPriData->mSemQuit);
        AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
        SemTimedWait(&mPriData->mSemQuit, -1);
        pthread_join(mPriData->mThreadId, &status);
    }

    if(mPriData->mDemux != NULL)
        DemuxCompDestroy(mPriData->mDemux);

    if(mPriData->mPlayer != NULL)
        PlayerDestroy(mPriData->mPlayer);

    if(mPriData->mMessageQueue != NULL)
    {
        AwMessageQueueDestroy(mPriData->mMessageQueue);
        mPriData->mMessageQueue = NULL;
    }

    pthread_mutex_destroy(&mPriData->mMutexMediaInfo);
    pthread_mutex_destroy(&mPriData->mMutexStatus);
    sem_destroy(&mPriData->mSemSetDataSource);
    sem_destroy(&mPriData->mSemPrepare);
    sem_destroy(&mPriData->mSemStart);
    sem_destroy(&mPriData->mSemStop);
    sem_destroy(&mPriData->mSemPause);
    sem_destroy(&mPriData->mSemReset);
    sem_destroy(&mPriData->mSemQuit);
    sem_destroy(&mPriData->mSemSeek);
    sem_destroy(&mPriData->mSemSetSurface);
    sem_destroy(&mPriData->mSemSetAudioSink);
    sem_destroy(&mPriData->mSemPrepareFinish);

    if(mPriData->mMediaInfo != NULL)
        clearMediaInfo();

    if(mPriData->mSourceUrl != NULL)
        free(mPriData->mSourceUrl);

    if(mPriData->mSourceFd != -1)
        ::close(mPriData->mSourceFd);

    if(mPriData->mIndexFileFdOfIndexSubtitle != -1)
        ::close(mPriData->mIndexFileFdOfIndexSubtitle);

    #if(MUTE_DRM_WHEN_HDMI_FLAG)
    if(mPriData->mHDMIListener) {
        mPriData->mHDMIListener->setNotifyCallback(0, 0);
        mPriData->mHDMIListener->stop();
        delete mPriData->mHDMIListener;
        mPriData->mHDMIListener = NULL;
    }
    #endif
    callbackProcess(AWPLAYER_MESSAGE_PLAYER_AUDIORAWPLAY, (void*)param_release);

    if (mPriData)
    {
        free(mPriData);
        mPriData = NULL;
    }
}


status_t AwPlayer::initCheck()
{
    logv("initCheck");

    if(mPriData->mPlayer == NULL || mPriData->mDemux == NULL || mPriData->mThreadCreated == 0)
    {
        loge("initCheck() fail, AwPlayer::mplayer = %p, AwPlayer::mDemux = %p",
                mPriData->mPlayer, mPriData->mDemux);
        return NO_INIT;
    }
    else
        return OK;
}


status_t AwPlayer::setUID(uid_t uid)
{
    logv("setUID(), uid = %d", uid);
    mPriData->mUID = uid;
    return OK;
}

#if (CONFIG_OS_VERSION >= OPTION_OS_VERSION_ANDROID_5_0)
status_t AwPlayer::setDataSource(const sp<IMediaHTTPService> &httpService,
                                const char* pUrl,
                                const KeyedVector<String8, String8>* pHeaders)
#else
status_t AwPlayer::setDataSource(const char* pUrl,
                                const KeyedVector<String8, String8>* pHeaders)
#endif
{
    AwMessage msg;
    status_t ret;

    if(pUrl == NULL)
    {
        loge("setDataSource(url), url=NULL");
        return BAD_TYPE;
    }


    logd("setDataSource(url), url='%s'", pUrl);

#if (CONFIG_OS_VERSION >= OPTION_OS_VERSION_ANDROID_5_0)

    mPriData->mHTTPService.clear();
    mPriData->mHTTPService = httpService;

    //* send a set data source message.
    setMessage(&msg,
               /* message id. */
               AWPLAYER_COMMAND_SET_SOURCE,
               /* params[0] = &mSemSetDataSource. */
               (uintptr_t)&mPriData->mSemSetDataSource,
               /* params[1] = &mSetDataSourceReply. */
               (uintptr_t)&mPriData->mSetDataSourceReply,
               /* params[2] = SOURCE_TYPE_URL. */
               SOURCE_TYPE_URL,
               //* params[3] = pUrl.
               (uintptr_t)pUrl,
               //* params[4] = KeyedVector<String8, String8>* pHeaders;
               (uintptr_t)pHeaders,
               //* params[5] = mHTTPService.get();
               (uintptr_t)(mPriData->mHTTPService.get()));
#else

    //* send a set data source message.
    setMessage(&msg,
               //* message id.
               AWPLAYER_COMMAND_SET_SOURCE,
               //* params[0] = &mSemSetDataSource.
               (uintptr_t)&mPriData->mSemSetDataSource,
               //* params[1] = &mSetDataSourceReply.
               (uintptr_t)&mPriData->mSetDataSourceReply,
               //* params[2] = SOURCE_TYPE_URL.
               SOURCE_TYPE_URL,
               //* params[3] = pUrl.
               (uintptr_t)pUrl,
               //* params[4] = KeyedVector<String8, String8>* pHeaders;
               (uintptr_t)pHeaders);
#endif

    AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
    SemTimedWait(&mPriData->mSemSetDataSource, -1);

    if(mPriData->mSetDataSourceReply != OK)
        return (status_t)mPriData->mSetDataSourceReply;

    //* for local file, I think we should ack like file descriptor source,
    //* so here we call prepare().
    //* local file list: 'bdmv://---',  '/---'
    if (strncmp(pUrl, "bdmv://", 7) == 0 || strncmp(pUrl, "file://", 7) == 0 || pUrl[0] == '/')
    {
        //* for local file source set as a file descriptor,
        //* the application will call invoke() by command INVOKE_ID_GET_TRACK_INFO
        //* to get track info, so we need call prepare() here to obtain media information before
        //* application call prepareAsync().
        //* here I think for local file source set as a url, we should ack the same as the file
        //* descriptor case.
        ret = prepare();
        if (ret != OK)
        {
            loge("prepare failure, ret(%d)", ret);
        }
        return ret;
    }
    else
        return OK;
}


//* Warning: The filedescriptor passed into this method will only be valid until
//* the method returns, if you want to keep it, dup it!
status_t AwPlayer::setDataSource(int fd, int64_t offset, int64_t length)
{
    AwMessage msg;
    status_t ret;

    logd("setDataSource(fd), fd=%d", fd);

    //* send a set data source message.
    setMessage(&msg,
               //* message id.
               AWPLAYER_COMMAND_SET_SOURCE,
               //* params[0] = &mSemSetDataSource.
               (uintptr_t)&mPriData->mSemSetDataSource,
               //* params[1] = &mSetDataSourceReply.
               (uintptr_t)&mPriData->mSetDataSourceReply,
               //* params[2] = SOURCE_TYPE_FD.
               SOURCE_TYPE_FD,
               //* params[3] = fd.
               fd,
               //* params[4] = high 32 bits of offset.
               (uintptr_t)(offset>>32),
               //* params[5] = low 32 bits of offset.
               (uintptr_t)(offset & 0xffffffff),
               //* params[6] = high 32 bits of length.
               (uintptr_t)(length>>32),
               //* params[7] = low 32 bits of length.
               (uintptr_t)(length & 0xffffffff));
    AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
    SemTimedWait(&mPriData->mSemSetDataSource, -1);

    if(mPriData->mSetDataSourceReply != OK)
        return (status_t)mPriData->mSetDataSourceReply;

    //* for local files, the application will call invoke() by command INVOKE_ID_GET_TRACK_INFO
    //* to get track info, so we need call prepare() here to obtain media information before
    //* application call prepareAsync().
    ret = prepare();
    if (ret != OK)
    {
        loge("prepare failure, ret(%d)", ret);
    }
    return ret;
}


status_t AwPlayer::setDataSource(const sp<IStreamSource> &source)
{
    AwMessage msg;
    logd("setDataSource(IStreamSource)");

    unsigned int numBuffer, bufferSize;
    const char *suffix = "";
    if(!strcmp(mPriData->strApkName, "com.hpplay.happyplay.aw"))
    {
        numBuffer = 32;
        bufferSize = 32*1024;
        suffix = ".awts";
    }
    else if(!strcmp(mPriData->strApkName, "com.softwinner.miracastReceiver"))
    {
        numBuffer = 1024;
        bufferSize = 188*8;
        suffix = ".ts";
    }
    else
    {
        CDX_LOGW("this type is unknown.");
        numBuffer = 16;
        bufferSize = 4*1024;
    }
    CdxStreamT *stream = StreamingSourceOpen(source, numBuffer, bufferSize);
    if(stream == NULL)
    {
        loge("StreamingSourceOpen fail!");
        return UNKNOWN_ERROR;
    }
    mPriData->mApplicationType = MIRACAST;
    PlayerConfigDispErrorFrame(mPriData->mPlayer, 1);

    char str[128];
    sprintf(str, "customer://%p%s",stream, suffix);
    //* send a set data source message.
    setMessage(&msg,
               AWPLAYER_COMMAND_SET_SOURCE,         //* message id.
               (uintptr_t)&mPriData->mSemSetDataSource,    //* params[0] = &mSemSetDataSource.
               (uintptr_t)&mPriData->mSetDataSourceReply,  //* params[1] = &mSetDataSourceReply.
               SOURCE_TYPE_ISTREAMSOURCE,           //* params[2] = SOURCE_TYPE_ISTREAMSOURCE.
               (uintptr_t)str);                     //* params[3] = uri of IStreamSource.
    AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
    SemTimedWait(&mPriData->mSemSetDataSource, -1);
    return (status_t)mPriData->mSetDataSourceReply;
}


#if CONFIG_OS_VERSION != OPTION_OS_VERSION_ANDROID_4_2
    //* android 4.4 use IGraphicBufferProducer instead of ISurfaceTexture in android 4.2.
status_t AwPlayer::setVideoSurfaceTexture(const sp<IGraphicBufferProducer>& bufferProducer)
{
    AwMessage msg;

    logv("setVideoSurfaceTexture, surface = %p", bufferProducer.get());

    //* send a set surface message.
    setMessage(&msg,
               AWPLAYER_COMMAND_SET_SURFACE,        //* message id.
               (uintptr_t)&mPriData->mSemSetSurface,       //* params[0] = &mSemSetSurface.
               (uintptr_t)&mPriData->mSetSurfaceReply,     //* params[1] = &mSetSurfaceReply.
               (uintptr_t)bufferProducer.get()); //* params[2] = pointer to ISurfaceTexture.
    AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
    SemTimedWait(&mPriData->mSemSetSurface, -1);

    return (status_t)mPriData->mSetSurfaceReply;
}
#else
status_t AwPlayer::setVideoSurfaceTexture(const sp<ISurfaceTexture> &surfaceTexture)
{
    AwMessage msg;

    logv("setVideoSurfaceTexture, surface = %p", surfaceTexture.get());

    //* send a set surface message.
    setMessage(&msg,
               AWPLAYER_COMMAND_SET_SURFACE,        //* message id.
               (uintptr_t)&mPriData->mSemSetSurface,       //* params[0] = &mSemSetSurface.
               (uintptr_t)&mPriData->mSetSurfaceReply,     //* params[1] = &mSetSurfaceReply.
               (uintptr_t)surfaceTexture.get()); //* params[2] = pointer to ISurfaceTexture.
    AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
    SemTimedWait(&mPriData->mSemSetSurface, -1);

    return (status_t)mPriData->mSetSurfaceReply;
}
#endif


void AwPlayer::setAudioSink(const sp<AudioSink> &audioSink)
{
    AwMessage msg;

    logv("setAudioSink");

    //* send a set audio sink message.
    setMessage(&msg,
               AWPLAYER_COMMAND_SET_AUDIOSINK,      //* message id.
               (uintptr_t)&mPriData->mSemSetAudioSink,     //* params[0] = &mSemSetAudioSink.
               (uintptr_t)&mPriData->mSetAudioSinkReply,   //* params[1] = &mSetAudioSinkReply.
               (uintptr_t)audioSink.get());      //* params[2] = pointer to AudioSink.
    AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
    SemTimedWait(&mPriData->mSemSetAudioSink, -1);

    return;
}


status_t AwPlayer::prepareAsync()
{
    AwMessage msg;

    logd("prepareAsync");

    //* send a prepare.
    setMessage(&msg,
               AWPLAYER_COMMAND_PREPARE,        //* message id.
               (uintptr_t)&mPriData->mSemPrepare,      //* params[0] = &mSemPrepare.
               (uintptr_t)&mPriData->mPrepareReply,    //* params[1] = &mPrepareReply.
               0);                              //* params[2] = mPrepareSync.
    AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
    SemTimedWait(&mPriData->mSemPrepare, -1);
    return (status_t)mPriData->mPrepareReply;
}


status_t AwPlayer::prepare()
{
    AwMessage msg;

    logd("prepare");

    //* clear the mSemPrepareFinish semaphore.
    while(sem_trywait(&mPriData->mSemPrepareFinish) == 0);

    //* send a prepare message.
    setMessage(&msg,
               AWPLAYER_COMMAND_PREPARE,        //* message id.
               (uintptr_t)&mPriData->mSemPrepare,      //* params[0] = &mSemPrepare.
               (uintptr_t)&mPriData->mPrepareReply,    //* params[1] = &mPrepareReply.
               1);                              //* params[2] = mPrepareSync.
    AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
    SemTimedWait(&mPriData->mSemPrepare, -1);

    if(mPriData->mPrepareReply == (int)OK)
    {
        //* wait for the prepare finish.
        SemTimedWait(&mPriData->mSemPrepareFinish, -1);
        return (status_t)mPriData->mPrepareFinishResult;
    }
    else
    {
        //* call DemuxCompPrepareAsync() fail, or status error.
        return (status_t)mPriData->mPrepareReply;
    }
}

int pauseMediaScanner(const char* strApkName)
{
    CEDARX_UNUSE(strApkName);
    return property_set("mediasw.stopscaner", "1");
}

status_t AwPlayer::start()
{
    AwMessage msg;

    logd("start");

    pauseMediaScanner(mPriData->strApkName);
    enableMediaBoost(mPriData->mMediaInfo, &(mPriData->mUartLogLevel));

    char value[PROP_VALUE_MAX] = {0};
    property_get("ro.media.keep_last_frame", value, "0");
    if(strcmp("1", value)==0)
    {
        int ret = PlayerSetHoldLastPicture(mPriData->mPlayer, 1);
        if(ret == -1)
        {
            loge("set hold last picture failed!");
        }
    }

    //* set cache params for specific application.
    //* some apk like 'topicvideo' will switch media source and restart playing
    //* if wait too long for buffering, so we can not buffer too much data(cost long time).
    //* in this case we use specific cache policy instead of the default cache params.
    if(mPriData->mStatus == AWPLAYER_STATUS_PREPARED)
    {
        CacheParamConfig* cp;

        cp = &CacheParamForSpecificApk[0];
        while(cp->strApkName != NULL)
        {
            if(strstr(mPriData->strApkName, cp->strApkName) != NULL)
            {
                logi("use specific cache params for %s, policy = %d, startPlaySize = %d, \
                        startPlayTimeMs = %d, cacheBufferSize = %d",
                    cp->strApkName, cp->eCachePolicy, cp->nStartPlaySize,
                    cp->nStartPlayTimeMs, cp->nCacheBufferSize);
                DemuxCompSetCachePolicy(mPriData->mDemux,
                                        (enum ECACHEPOLICY)cp->eCachePolicy,
                                        cp->nStartPlaySize,
                                        cp->nStartPlayTimeMs,
                                        cp->nCacheBufferSize);
                break;
            }
            cp++;
        }
    }

    //* send a start message.
    setMessage(&msg,
               AWPLAYER_COMMAND_START,        //* message id.
               (uintptr_t)&mPriData->mSemStart,      //* params[0] = &mSemStart.
               (uintptr_t)&mPriData->mStartReply);   //* params[1] = &mStartReply.
    AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
    SemTimedWait(&mPriData->mSemStart, -1);
    return (status_t)mPriData->mStartReply;
}


status_t AwPlayer::stop()
{
    AwMessage msg;

    logw("stop");

    //* send a stop message.
    setMessage(&msg,
               AWPLAYER_COMMAND_STOP,        //* message id.
               (uintptr_t)&mPriData->mSemStop,      //* params[0] = &mSemStop.
               (uintptr_t)&mPriData->mStopReply);   //* params[1] = &mStopReply.
    AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
    SemTimedWait(&mPriData->mSemStop, -1);
    return (status_t)mPriData->mStopReply;
}


status_t AwPlayer::pause()
{
    AwMessage msg;

    logd("pause");

    //* send a pause message.
    setMessage(&msg,
               AWPLAYER_COMMAND_PAUSE,        //* message id.
               (uintptr_t)&mPriData->mSemPause,      //* params[0] = &mSemPause.
               (uintptr_t)&mPriData->mPauseReply);   //* params[1] = &mPauseReply.
    AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
    SemTimedWait(&mPriData->mSemPause, -1);
    return (status_t)mPriData->mPauseReply;
}

status_t AwPlayer::seekTo(int nSeekTimeMs)
{
    AwMessage msg;

    logd("seekTo [%dms]", nSeekTimeMs);

    //* send a start message.
    setMessage(&msg,
               AWPLAYER_COMMAND_SEEK,        //* message id.
               (uintptr_t)&mPriData->mSemSeek,      //* params[0] = &mSemSeek.
               (uintptr_t)&mPriData->mSeekReply,    //* params[1] = &mSeekReply.
               nSeekTimeMs,                  //* params[2] = mSeekTime.
               0);                           //* params[3] = mSeekSync.
    AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
    SemTimedWait(&mPriData->mSemSeek, -1);
    return (status_t)mPriData->mSeekReply;
}

status_t AwPlayer::setSpeed(int nSpeed)
{
    CEDARX_UNUSE(nSpeed);
    return 0;
}

int resumeMediaScanner(const char* strApkName)
{
    CEDARX_UNUSE(strApkName);
    return property_set("mediasw.stopscaner", "0");
}

status_t AwPlayer::reset()
{
    AwMessage msg;

    logw("reset...");

    disableMediaBoost(mPriData->mUartLogLevel);
    resumeMediaScanner(mPriData->strApkName);

    //* send a start message.
    setMessage(&msg,
               AWPLAYER_COMMAND_RESET,       //* message id.
               (uintptr_t)&mPriData->mSemReset,     //* params[0] = &mSemReset.
               (uintptr_t)&mPriData->mResetReply);  //* params[1] = &mResetReply.
    AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
    SemTimedWait(&mPriData->mSemReset, -1);
    return (status_t)mPriData->mResetReply;
}


bool AwPlayer::isPlaying()
{
    logv("isPlaying");
    if(mPriData->mStatus == AWPLAYER_STATUS_STARTED ||
        (mPriData->mStatus == AWPLAYER_STATUS_COMPLETE && mPriData->mLoop != 0))
        return true;
    else
        return false;
}


status_t AwPlayer::getCurrentPosition(int* msec)
{
    int64_t nPositionUs;

    logv("getCurrentPosition");

    if(mPriData->mStatus == AWPLAYER_STATUS_PREPARED ||
       mPriData->mStatus == AWPLAYER_STATUS_STARTED  ||
       mPriData->mStatus == AWPLAYER_STATUS_PAUSED   ||
       mPriData->mStatus == AWPLAYER_STATUS_COMPLETE)
    {
        if(mPriData->mSeeking != 0)
        {
            *msec = mPriData->mSeekTime;
            return OK;
        }

        //* in complete status, the prepare() method maybe called and it change the media info.
        pthread_mutex_lock(&mPriData->mMutexMediaInfo);
        if(mPriData->mMediaInfo != NULL)
        {
            //* ts stream's pts is not started at 0.
            if(mPriData->mMediaInfo->eContainerType == CONTAINER_TYPE_TS ||
               mPriData->mMediaInfo->eContainerType == CONTAINER_TYPE_BD ||
               mPriData->mMediaInfo->eContainerType == CONTAINER_TYPE_HLS)
                nPositionUs = PlayerGetPosition(mPriData->mPlayer);
            else
                nPositionUs = PlayerGetPts(mPriData->mPlayer);

            *msec = (nPositionUs + 500)/1000;
            if(*msec >= mPriData->mMediaInfo->nDurationMs &&
                    mPriData->mMediaInfo->nDurationMs != 0)
            {
                *msec = mPriData->mMediaInfo->nDurationMs;
            }
            pthread_mutex_unlock(&mPriData->mMutexMediaInfo);
            return OK;
        }
        else
        {
            loge("getCurrentPosition() fail, mMediaInfo==NULL.");
            *msec = 0;
            pthread_mutex_unlock(&mPriData->mMutexMediaInfo);
            return OK;
        }
    }
    else
    {
        *msec = 0;
        if(mPriData->mStatus == AWPLAYER_STATUS_ERROR)
            return INVALID_OPERATION;
        else
            return OK;
    }
}


status_t AwPlayer::getDuration(int *msec)
{
    logv("getDuration");

    if(mPriData->mStatus == AWPLAYER_STATUS_PREPARED ||
       mPriData->mStatus == AWPLAYER_STATUS_STARTED  ||
       mPriData->mStatus == AWPLAYER_STATUS_PAUSED   ||
       mPriData->mStatus == AWPLAYER_STATUS_STOPPED  ||
       mPriData->mStatus == AWPLAYER_STATUS_COMPLETE)
    {
        //* in complete status, the prepare() method maybe called and it change the media info.
        pthread_mutex_lock(&mPriData->mMutexMediaInfo);
        if(mPriData->mMediaInfo != NULL)
        {
            *msec = mPriData->mMediaInfo->nDurationMs;

            //* we should set *msec to -1 when can not get the right duration,
            //* or the live.pptv.com can not play
            if((*msec) == 0)
            {
                *msec = -1;
            }
        }
        else
        {
            loge("getCurrentPosition() fail, mPriData->mMediaInfo==NULL.");
            *msec = -1;
        }
        pthread_mutex_unlock(&mPriData->mMutexMediaInfo);
        return OK;
    }
    else
    {
        loge("invalid getDuration() call, player not in valid status.");
        return INVALID_OPERATION;
    }
}


status_t AwPlayer::setLooping(int loop)
{
    logv("setLooping");

    if(mPriData->mStatus == AWPLAYER_STATUS_ERROR)
        return INVALID_OPERATION;

    mPriData->mLoop = loop;
    return OK;
}


player_type AwPlayer::playerType()
{
    logv("playerType");
    return AW_PLAYER;
}


status_t AwPlayer::invoke(const Parcel &request, Parcel *reply)
{
    int nMethodId;
    int ret;

    logv("invoke()");

    ret = request.readInt32(&nMethodId);
    if(ret != OK)
        return ret;

    switch(nMethodId)
    {
        case INVOKE_ID_GET_TRACK_INFO:  //* get media stream counts.
        {
            logi("invode::INVOKE_ID_GET_TRACK_INFO");

            pthread_mutex_lock(&mPriData->mMutexMediaInfo);
            if(mPriData->mStatus == AWPLAYER_STATUS_IDLE ||
               mPriData->mStatus == AWPLAYER_STATUS_INITIALIZED ||
               mPriData->mMediaInfo == NULL)
            {
                pthread_mutex_unlock(&mPriData->mMutexMediaInfo);
                return NO_INIT;
            }
            else
            {
                int         i;
                int         nTrackCount;
                const char* lang;

                nTrackCount = mPriData->mMediaInfo->nVideoStreamNum +
                              mPriData->mMediaInfo->nAudioStreamNum +
                              mPriData->mMediaInfo->nSubtitleStreamNum;
#if AWPLAYER_CONFIG_DISABLE_VIDEO
                nTrackCount -= mPriData->mMediaInfo->nVideoStreamNum;
#endif
#if AWPLAYER_CONFIG_DISABLE_AUDIO
                nTrackCount -= mPriData->mMediaInfo->nAudioStreamNum;
#endif
#if AWPLAYER_CONFIG_DISABLE_SUBTITLE
                nTrackCount -= mPriData->mMediaInfo->nSubtitleStreamNum;
#endif
                reply->writeInt32(nTrackCount);

#if !AWPLAYER_CONFIG_DISABLE_VIDEO
                for(i=0; i<mPriData->mMediaInfo->nVideoStreamNum; i++)
                {
#if CONFIG_PRODUCT == OPTION_PRODUCT_TVBOX
                    reply->writeInt32(4);
#else
                    reply->writeInt32(2);
#endif
                    reply->writeInt32(MEDIA_TRACK_TYPE_VIDEO);
#if CONFIG_OS_VERSION == OPTION_OS_VERSION_ANDROID_6_0
                    reply->writeString16(String16("video/"));
#endif
                    lang = " ";
                    reply->writeString16(String16(lang));
#if CONFIG_PRODUCT == OPTION_PRODUCT_TVBOX
                    reply->writeInt32(mPriData->mMediaInfo->pVideoStreamInfo[i].bIs3DStream);
                    /* Please refer to the "enum EVIDEOCODECFORMAT" in
                     * "liballwinner\library\codec\video\decoder\vdecoder.h"
                     */
                    reply->writeInt32(mPriData->mMediaInfo->pVideoStreamInfo[i].eCodecFormat);
#endif
                }
#endif


#if !AWPLAYER_CONFIG_DISABLE_AUDIO
                for(i=0; i<mPriData->mMediaInfo->nAudioStreamNum; i++)
                {
#if CONFIG_PRODUCT == OPTION_PRODUCT_TVBOX
                    reply->writeInt32(6);
#else
                    reply->writeInt32(2);
#endif
                    reply->writeInt32(MEDIA_TRACK_TYPE_AUDIO);
#if CONFIG_OS_VERSION == OPTION_OS_VERSION_ANDROID_6_0
                    reply->writeString16(String16("audio/"));
#endif
                    if(mPriData->mMediaInfo->pAudioStreamInfo[i].strLang[0] != 0)
                    {
                        lang = (const char*)mPriData->mMediaInfo->pAudioStreamInfo[i].strLang;
                        reply->writeString16(String16(lang));
                    }
                    else
                    {
                        lang = "";
                        reply->writeString16(String16(lang));
                    }
#if CONFIG_PRODUCT == OPTION_PRODUCT_TVBOX
                    reply->writeInt32(mPriData->mMediaInfo->pAudioStreamInfo[i].nChannelNum);
                    reply->writeInt32(mPriData->mMediaInfo->pAudioStreamInfo[i].nSampleRate);
                    reply->writeInt32(mPriData->mMediaInfo->pAudioStreamInfo[i].nAvgBitrate);
                    /* Please refer to the "enum EAUDIOCODECFORMAT" in
                     * "liballwinner\library\codec\video\decoder\adecoder.h"
                     */
                    reply->writeInt32(mPriData->mMediaInfo->pAudioStreamInfo[i].eCodecFormat);

#endif
                }
#endif

#if !AWPLAYER_CONFIG_DISABLE_SUBTITLE
                for(i=0; i<mPriData->mMediaInfo->nSubtitleStreamNum; i++)
                {
#if CONFIG_PRODUCT == OPTION_PRODUCT_TVBOX
                    reply->writeInt32(4);
#else
                    reply->writeInt32(2);
#endif
                    reply->writeInt32(MEDIA_TRACK_TYPE_TIMEDTEXT);
#if CONFIG_OS_VERSION == OPTION_OS_VERSION_ANDROID_6_0
                    reply->writeString16(String16("text/3gpp-tt"));
#endif
                    if(mPriData->mMediaInfo->pSubtitleStreamInfo[i].strLang[0] != 0)
                    {
                        lang = (const char*)mPriData->mMediaInfo->pSubtitleStreamInfo[i].strLang;
                        reply->writeString16(String16(lang));
                    }
                    else
                    {
                        lang = "";
                        reply->writeString16(String16(lang));
                    }
#if CONFIG_PRODUCT == OPTION_PRODUCT_TVBOX
                    /* Please refer to the "ESubtitleCodec" in
                     * "liballwinner\library\codec\video\decoder\sdecoder.h"
                     */
                    reply->writeInt32(mPriData->mMediaInfo->pSubtitleStreamInfo[i].eCodecFormat);
                    reply->writeInt32(mPriData->mMediaInfo->pSubtitleStreamInfo[i].bExternal);
#endif
                }
#endif

                pthread_mutex_unlock(&mPriData->mMutexMediaInfo);
                return OK;
            }
        }

        case INVOKE_ID_ADD_EXTERNAL_SOURCE:
        case INVOKE_ID_ADD_EXTERNAL_SOURCE_FD:
        {
            logi("invode::INVOKE_ID_ADD_EXTERNAL_SOURCE");

            SubtitleStreamInfo* pStreamInfo;
            SubtitleStreamInfo* pNewStreamInfo;
            int                 nStreamNum;
            int                 i;
            int                 fd;
            int                 nOffset;
            int                 nLength;
            int                 fdSub;

            pNewStreamInfo = NULL;
            nStreamNum     = 0;

            if(mPriData->mStatus != AWPLAYER_STATUS_PREPARED || mPriData->mMediaInfo == NULL)
            {
                loge("can not add external text source, player not in prepared status \
                    or there is no media stream.");
                return NO_INIT;
            }

            if(nMethodId == INVOKE_ID_ADD_EXTERNAL_SOURCE)
            {
                //* string values written in Parcel are UTF-16 values.
                String8 uri(request.readString16());
                String8 mimeType(request.readString16());

                //* if mimeType == "application/sub" and mIndexFileHasBeenSet == 0,
                //* the .sub file is a common .sub subtitle, not index+sub subtitle.

                if(strcmp((char*)mimeType.string(), "application/sub") == 0 &&
                   mPriData->mIndexFileHasBeenSet == 1)
                {
                    //* the .sub file of index+sub subtitle.
                    //* no need to use the .sub file url, because subtitle decoder will
                    //* find the .sub file by itself by using the .idx file's url.
                    //* mimetype "application/sub" is defined in
                    //* "android/base/media/java/android/media/MediaPlayer.java".
                    //* clear the flag for adding more subtitle.
                    mPriData->mIndexFileHasBeenSet = 0;
                    return OK;
                }
                else if(strcmp((char*)mimeType.string(), "application/idx+sub"))
                {
                    //* set this flag to process the .sub file passed in at next call.
                    mPriData->mIndexFileHasBeenSet = 1;
                }

                //* probe subtitle info
                PlayerProbeSubtitleStreamInfo(uri.string(), &nStreamNum, &pNewStreamInfo);
            }
            else
            {
                fd      = request.readFileDescriptor();
                nOffset = request.readInt64();
                nLength = request.readInt64();
                fdSub   = -1;
                String8 mimeType(request.readString16());

                //* if mimeType == "application/sub" and mIndexFileHasBeenSet == 0,
                //* the .sub file is a common .sub subtitle, not index+sub subtitle.

                if(strcmp((char*)mimeType.string(), "application/idx-sub") == 0)
                {
                    //* the .idx file of index+sub subtitle.
                    mPriData->mIndexFileFdOfIndexSubtitle = dup(fd);
                    mPriData->mIndexFileHasBeenSet = 1;
                    return OK;
                }
                else if(strcmp((char*)mimeType.string(), "application/sub") == 0 &&
                        mPriData->mIndexFileHasBeenSet == 1)
                {
                    //* the .sub file of index+sub subtitle.
                    //* for index+sub subtitle, PlayerProbeSubtitleStreamInfoFd() method need
                    //* the .idx file's descriptor for probe.
                    fdSub = fd;
                    fd    = mPriData->mIndexFileFdOfIndexSubtitle;
                    mPriData->mIndexFileFdOfIndexSubtitle = -1;
                    mPriData->mIndexFileHasBeenSet = 0;
                }

                //* probe subtitle info
                PlayerProbeSubtitleStreamInfoFd(fd,nOffset,nLength, &nStreamNum, &pNewStreamInfo);

                if(nStreamNum > 0 && pNewStreamInfo[0].eCodecFormat == SUBTITLE_CODEC_IDXSUB)
                {
                    if(fdSub >= 0)
                    {
                        //* for index+sub subtitle,
                        //* we set the .sub file's descriptor to pNewStreamInfo[i].fdSub.
                        for(i=0; i<nStreamNum; i++)
                            pNewStreamInfo[i].fdSub = dup(fdSub);
                    }
                    else
                    {
                        loge("index sub subtitle stream without .sub file fd.");
                        for(i=0; i<nStreamNum; i++)
                        {
                            if(pNewStreamInfo[i].fd >= 0)
                            {
                                ::close(pNewStreamInfo[i].fd);
                                pNewStreamInfo[i].fd = -1;
                            }
                        }
                        free(pNewStreamInfo);
                        pNewStreamInfo = NULL;
                        nStreamNum = 0;
                    }
                }

                //* fdSub is the file descriptor of .sub file of a index+sub subtitle.
                if(fdSub >= 0)
                {
                    //* close the file descriptor of .idx file, we dup it when
                    //* INVOKE_ID_ADD_EXTERNAL_SOURCE_FD is called to set the .idx file
                    if(fd >= 0)
                        ::close(fd);
                }
            }

            if(pNewStreamInfo == NULL || nStreamNum == 0)
            {
                loge("PlayerProbeSubtitleStreamInfo failed!");
                return UNKNOWN_ERROR;
            }

            pthread_mutex_lock(&mPriData->mMutexMediaInfo);

            //* set reference video size.
            if(mPriData->mMediaInfo->pVideoStreamInfo != NULL)
            {
                for(i=0; i<nStreamNum; i++)
                {
                    if(pNewStreamInfo[i].nReferenceVideoFrameRate == 0)
                        pNewStreamInfo[i].nReferenceVideoFrameRate =
                                mPriData->mMediaInfo->pVideoStreamInfo->nFrameRate;

                    if(pNewStreamInfo[i].nReferenceVideoHeight == 0 ||
                       pNewStreamInfo[i].nReferenceVideoWidth == 0)
                    {
                        pNewStreamInfo[i].nReferenceVideoHeight =
                                mPriData->mMediaInfo->pVideoStreamInfo->nHeight;
                        pNewStreamInfo[i].nReferenceVideoWidth =
                                mPriData->mMediaInfo->pVideoStreamInfo->nWidth;
                    }
                }
            }

            //* add stream info to the mMediaInfo,
            //* put external subtitle streams ahead of the embedded subtitle streams.
            if(mPriData->mMediaInfo->pSubtitleStreamInfo == NULL)
            {
                mPriData->mMediaInfo->pSubtitleStreamInfo = pNewStreamInfo;
                mPriData->mMediaInfo->nSubtitleStreamNum  = nStreamNum;
                pNewStreamInfo = NULL;
                nStreamNum = 0;
            }
            else
            {
                int n = mPriData->mMediaInfo->nSubtitleStreamNum + nStreamNum;
                pStreamInfo = (SubtitleStreamInfo *)malloc(sizeof(*pStreamInfo) * n);
                if(pStreamInfo == NULL)
                {
                    loge("invode::INVOKE_ID_ADD_EXTERNAL_SOURCE fail, can not malloc memory.");
                    for(i=0; i<nStreamNum; i++)
                    {
                        if(pNewStreamInfo[i].pUrl != NULL)
                        {
                            free(pNewStreamInfo[i].pUrl);
                            pNewStreamInfo[i].pUrl = NULL;
                        }

                        if(pNewStreamInfo[i].fd >= 0)
                        {
                            ::close(pNewStreamInfo[i].fd);
                            pNewStreamInfo[i].fd = -1;
                        }

                        if(pNewStreamInfo[i].fdSub >= 0)
                        {
                            ::close(pNewStreamInfo[i].fdSub);
                            pNewStreamInfo[i].fdSub = -1;
                        }
                    }

                    free(pNewStreamInfo);
                    pthread_mutex_unlock(&mPriData->mMutexMediaInfo);
                    return NO_MEMORY;
                }

                //* make the internal subtitle in front of external subtitle
                memcpy(&pStreamInfo[mPriData->mMediaInfo->nSubtitleStreamNum],
                        pNewStreamInfo, sizeof(SubtitleStreamInfo)*nStreamNum);
                memcpy(pStreamInfo, mPriData->mMediaInfo->pSubtitleStreamInfo,
                        sizeof(SubtitleStreamInfo)*mPriData->mMediaInfo->nSubtitleStreamNum);

                free(mPriData->mMediaInfo->pSubtitleStreamInfo);
                mPriData->mMediaInfo->pSubtitleStreamInfo = pStreamInfo;
                mPriData->mMediaInfo->nSubtitleStreamNum += nStreamNum;

                free(pNewStreamInfo);
                pNewStreamInfo = NULL;
                nStreamNum = 0;
            }

            //* re-arrange the stream index.
            for(i=0; i<mPriData->mMediaInfo->nSubtitleStreamNum; i++)
                mPriData->mMediaInfo->pSubtitleStreamInfo[i].nStreamIndex = i;

            //* set subtitle stream info to player again.
            //* here mMediaInfo != NULL, so initializePlayer() had been called.
            if(mPriData->mPlayer != NULL)
            {
                int nDefaultSubtitleIndex = -1;
                for(i=0; i<mPriData->mMediaInfo->nSubtitleStreamNum; i++)
                {
                    if(PlayerCanSupportSubtitleStream(mPriData->mPlayer,
                                &mPriData->mMediaInfo->pSubtitleStreamInfo[i]))
                    {
                        nDefaultSubtitleIndex = i;
                        break;
                    }
                }

                if(nDefaultSubtitleIndex < 0)
                {
                    logw("no subtitle stream supported.");
                    nDefaultSubtitleIndex = 0;
                }

                if(0 != PlayerSetSubtitleStreamInfo(mPriData->mPlayer,
                                                    mPriData->mMediaInfo->pSubtitleStreamInfo,
                                                    mPriData->mMediaInfo->nSubtitleStreamNum,
                                                    nDefaultSubtitleIndex))
                {
                    logw("PlayerSetSubtitleStreamInfo() fail, subtitle stream not supported.");
                }
            }

            pthread_mutex_unlock(&mPriData->mMutexMediaInfo);
            return OK;
        }

        case INVOKE_ID_SELECT_TRACK:
        case INVOKE_ID_UNSELECT_TRACK:
        {
            int nStreamIndex;
            int nTrackCount;

            nStreamIndex = request.readInt32();
            logd("invode::INVOKE_ID_SELECT_TRACK, stream index = %d", nStreamIndex);

            if(mPriData->mMediaInfo == NULL)
            {
                loge("can not switch audio or subtitle, there is no media stream.");
                return NO_INIT;
            }

            int nVideoStreamNum = mPriData->mMediaInfo->nVideoStreamNum;
            int nAudioStreamNum = mPriData->mMediaInfo->nAudioStreamNum;
            int nSubtitleStreamNum = mPriData->mMediaInfo->nSubtitleStreamNum;
            nTrackCount = nVideoStreamNum + nAudioStreamNum + nSubtitleStreamNum;

            if(nTrackCount == 0)
            {
                loge("can not switch audio or subtitle, there is no media stream.");
                return NO_INIT;
            }

            if(nStreamIndex >= nVideoStreamNum &&
                    nStreamIndex < (nVideoStreamNum + nAudioStreamNum))
            {
                if(nMethodId == INVOKE_ID_SELECT_TRACK)
                {
                    //* switch audio.
                    nStreamIndex -= nVideoStreamNum;
                    if(PlayerSwitchAudio(mPriData->mPlayer, nStreamIndex) != 0)
                    {
                        loge("can not switch audio, call PlayerSwitchAudio() return fail.");
                        return UNKNOWN_ERROR;
                    }
                }
                else
                {
                    loge("Deselect an audio track (%d) is not supported", nStreamIndex);
                    return INVALID_OPERATION;
                }

                return OK;
            }
            else if(nStreamIndex >= (nVideoStreamNum + nAudioStreamNum)
                    && nStreamIndex < nTrackCount)
            {
                if(nMethodId == INVOKE_ID_SELECT_TRACK)
                {
                    mPriData->mCurrentSelectTrackIndex = nStreamIndex;
                    //* enable subtitle.
                    mPriData->mIsSubtitleDisable = 0;

                    //* switch subtitle.
                    nStreamIndex -= (nVideoStreamNum + nAudioStreamNum);
                    if(PlayerSwitchSubtitle(mPriData->mPlayer, nStreamIndex) != 0)
                    {
                        loge("can not switch subtitle, call PlayerSwitchSubtitle() return fail.");
                        return UNKNOWN_ERROR;
                    }
                }
                else
                {
                    if(mPriData->mCurrentSelectTrackIndex != nStreamIndex)
                    {
                        logw("the unselectTrack is not right: %d, %d",
                              mPriData->mCurrentSelectTrackIndex,nStreamIndex);
                        return INVALID_OPERATION;
                    }
                    mPriData->mCurrentSelectTrackIndex = -1;
                    mPriData->mIsSubtitleDisable = 1; //* disable subtitle show.
                    sendEvent(MEDIA_TIMED_TEXT);//* clear all the displaying subtitle
                    //* clear the mSubtitleDisplayIds
                    memset(mPriData->mSubtitleDisplayIds,0xff,64*sizeof(unsigned int));
                    mPriData->mSubtitleDisplayIdsUpdateIndex = 0;
                }

                return OK;
            }

            if(nMethodId == INVOKE_ID_SELECT_TRACK)
            {
                loge("can not switch audio or subtitle to track %d, "
                        "stream index exceed.(%d stream in total).",
                    nStreamIndex, nTrackCount);
            }
            else
            {
                loge("can not unselect track %d, stream index exceed.(%d stream in total).",
                    nStreamIndex, nTrackCount);
            }
            return BAD_INDEX;
        }

        case INVOKE_ID_SET_VIDEO_SCALING_MODE:
        {
            int nStreamIndex;

            nStreamIndex = request.readInt32();
            logv("invode::INVOKE_ID_SET_VIDEO_SCALING_MODE");
            //* TODO.
            return OK;
        }

        case INVOKE_ID_SET_3D_MODE:
        {
            int ePicture3DMode;
            int eDisplay3DMode;
            int bIs3DStream;

            if(mPriData->mStatus != AWPLAYER_STATUS_PREPARED &&
               mPriData->mStatus != AWPLAYER_STATUS_STARTED &&
               mPriData->mStatus != AWPLAYER_STATUS_PAUSED &&
               mPriData->mStatus != AWPLAYER_STATUS_STOPPED)
            {
                loge("can not set 3d mode when not in prepared/started/paused/stopped status.");
                return INVALID_OPERATION;
            }

            if(mPriData->mMediaInfo == NULL || mPriData->mMediaInfo->nVideoStreamNum == 0)
            {
                loge("no video stream, can not set 3d mode.");
                return INVALID_OPERATION;
            }

            bIs3DStream = mPriData->mMediaInfo->pVideoStreamInfo[0].bIs3DStream;
            ret = request.readInt32(&ePicture3DMode);
            if(ret < 0)
                return ret;
            ret = request.readInt32(&eDisplay3DMode);
            if(ret < 0)
                return ret;

            if(bIs3DStream && ePicture3DMode != PICTURE_3D_MODE_TWO_SEPERATED_PICTURE)
            {
                loge("source picture 3d mode is doulbe stream, can not set it to other mode.");
                return INVALID_OPERATION;
            }

            if(PlayerSet3DMode(mPriData->mPlayer,
                        (enum EPICTURE3DMODE)ePicture3DMode,
                        (enum EDISPLAY3DMODE)eDisplay3DMode) != 0)
            {
                loge("call PlayerSet3DMode() return fail, "
                        "ePicture3DMode = %d, eDisplay3DMode = %d.",
                    ePicture3DMode, eDisplay3DMode);
                return UNKNOWN_ERROR;
            }

            return OK;
        }

        case INVOKE_ID_GET_3D_MODE:
        {
            enum EPICTURE3DMODE ePicture3DMode;
            enum EDISPLAY3DMODE eDisplay3DMode;
            int bIs3DStream;

            if(mPriData->mStatus != AWPLAYER_STATUS_PREPARED &&
               mPriData->mStatus != AWPLAYER_STATUS_STARTED &&
               mPriData->mStatus != AWPLAYER_STATUS_PAUSED &&
               mPriData->mStatus != AWPLAYER_STATUS_STOPPED)
            {
                loge("can not set 3d mode when not in prepared/started/paused/stopped status.");
                return INVALID_OPERATION;
            }

            if(mPriData->mMediaInfo == NULL || mPriData->mMediaInfo->nVideoStreamNum == 0)
            {
                loge("no video stream, can not get 3d mode.");
                return INVALID_OPERATION;
            }

            if(PlayerGet3DMode(mPriData->mPlayer, &ePicture3DMode, &eDisplay3DMode) != 0)
            {
                loge("call PlayerGet3DMode() return fail.");
                return UNKNOWN_ERROR;
            }

            reply->writeInt32(ePicture3DMode);
            reply->writeInt32(eDisplay3DMode);
            return OK;
        }
#if(PLAYREADY_DRM_INVOKE == 1)
        case INVOKE_ID_PLAYREADY_DRM:
        {
            PlayReady_Drm_Invoke(request, reply);
            return OK;
        }
#endif

#if( CONFIG_ALI_YUNOS == OPTION_ALI_YUNOS_YES)
        //*****************************************************************
        //* DemuxCompGetTSInfoYUNOS
        //* eMessageId:
        //*     YUNOS_TS_INFO                    = 1
        //*     YUNOS_MEDIA_INFO                = 2
        //*     YUNOS_HTTP_DOWNLOAD_ERROR_INFO    = 3
        //*     YUNOS_SOURCE                    = 4
        //*****************************************************************

        case YUNOS_SOURCE:
        {
            int eMessageId = 4;
            String8 uri(request.readString16());

            memcpy(mPriData->mYunOSUUID, uri.string(), 32/* strlen(uri.string())*/);
            CDX_LOGD(" YUNOSINVOKE mYunOSUUID[%s]",mPriData->mYunOSUUID);

            if(mPriData->mDemux)
            {
                DemuxCompGetTSInfoYUNOS(mPriData->mDemux, eMessageId, NULL,
                                        (void*)mPriData->mYunOSUUID);
            }

            mPriData->mYunOSInfoEndale = 1;
            return OK;
        }
        case YUNOS_TS_INFO:
        {
            if(mPriData->mMediaInfo && mPriData->mYunOSInfoEndale == 1)
            {

                if(mPriData->mMediaInfo->eContainerType == CONTAINER_TYPE_HLS)
                {
                    int eMessageId = 1;
                    char trings[2048];
                    char cdn_ip[128] = {0};
                    int event = 3005;//YUNOS_TS_INFO
                    cdx_int64 tsInfo[8] = {0};

                    if(mPriData->mDemux)
                        DemuxCompGetTSInfoYUNOS(mPriData->mDemux, eMessageId,
                                                (void*)tsInfo, (void*)cdn_ip);
                    cdx_int64    bitrate            =    tsInfo[0];
                    int         ts_num            =     (int)tsInfo[1];
                    cdx_int64    ts_length        =     tsInfo[2];
                    cdx_int64    ts_duration        =    tsInfo[3];
                    cdx_int64     ts_send_time     =    tsInfo[4]/1000;
                    cdx_int64    ts_first_btime    =    tsInfo[5]/1000;
                    cdx_int64    ts_last_btime    =    tsInfo[6]/1000;
                    cdx_int64    ts_nowUs        =    tsInfo[7]/1000;;

                    //* ts_traceid = UUID + timestamp + count
                    char ts_traceid[512];
                    sprintf(ts_traceid,"%s%13lld%4d",mPriData->mYunOSUUID,ts_nowUs,ts_num + 1000);

                    sprintf(trings,"event=%4d&ts_duration=%lld&ts_length=%lld"
                                   "&bitrate=%lld&ts_traceid=%s&ts_send_time=%lld"
                                   "&ts_first_btime=%lld&ts_last_btime=%lld"
                                   "&ts_num=%d&cdn_ip=%s",
                                    event, ts_duration, ts_length,
                                    bitrate, ts_traceid, ts_send_time,
                                    ts_first_btime, ts_last_btime, ts_num,cdn_ip);
                    reply->writeString16(String16(trings));
                    logv(" YUNOS_TS_INFO  trings[%s]",trings);
                }
            }
            return OK;
        }
        case YUNOS_HTTP_DOWNLOAD_ERROR_INFO:
        {
             if(mPriData->mMediaInfo && mPriData->mYunOSInfoEndale == 1)
             {

                if(mPriData->mMediaInfo->eContainerType == CONTAINER_TYPE_HLS)
                {
                    int eMessageId = 3;
                    int event = 4001;//YUNOS_HTTP_DOWNLOAD_ERROR_INFO
                    char trings[4608];
                    int error_code = mPriData->mYunOSErrorCode;
                    cdx_int64 tsInfo[2] = {0};
                    char http_res[4096];
                    if(mPriData->mDemux)
                        DemuxCompGetTSInfoYUNOS(mPriData->mDemux, eMessageId,
                                                (void*)&tsInfo,http_res);

                    int ts_num = (int)tsInfo[0];
                    cdx_int64 bitrate = tsInfo[1];

                    if(error_code == 3004 && http_res[0])
                    {
                        sprintf(trings,"event=%4d&error_code=%d&ts_num=%d"
                                       "&bitrate=%lld&http_res=%s",
                                       event, error_code, ts_num, bitrate, http_res);
                        reply->writeString16(String16(trings));
                    }
                    else
                    {
                        sprintf(trings,"event=%4d&error_code=%d&ts_num=%d&bitrate=%lld",
                                        event, error_code, ts_num, bitrate);
                        reply->writeString16(String16(trings));
                    }
                    reply->writeString16(String16(trings));
                    logv(" YUNOS_HTTP_DOWNLOAD_ERROR_INFO  trings[%s]",trings);
                }
            }
            return OK;
        }
        case YUNOS_MEDIA_INFO:
        {
            if(mPriData->mMediaInfo && mPriData->mYunOSInfoEndale == 1)
            {


                if(mPriData->mMediaInfo->eContainerType == CONTAINER_TYPE_HLS)
                {
                    int eMessageId = 2;
                    char trings[512];
                    char cdn_ip[128] = {0};
                    cdx_int64 tsInfo = 0;

                    if (mPriData->mDemux)
                        DemuxCompGetTSInfoYUNOS(mPriData->mDemux, eMessageId,
                                                (void*)&tsInfo, (void*)cdn_ip);

                    cdx_int64 bitrate = tsInfo;
                    char encode_type[] = "other";
                    if (mPriData->mMediaInfo->pVideoStreamInfo->eCodecFormat ==
                                                        VIDEO_CODEC_FORMAT_H265)
                        memcpy(encode_type,"H265",5);
                    else if(mPriData->mMediaInfo->pVideoStreamInfo->eCodecFormat ==
                                                        VIDEO_CODEC_FORMAT_H264)
                        memcpy(encode_type,"H264",5);

                    sprintf(trings,"encode_type=%s&cdn_ip=%s&bitrate=%lld",
                            encode_type, cdn_ip, bitrate);
                    reply->writeString16(String16(trings));
                    logv(" YUNOS_MEDIA_INFO  trings[%s]",trings);
                }
            }
            return OK;
        }
#endif

        default:
        {
            logv("unknown invode command %d", nMethodId);
            return UNKNOWN_ERROR;
        }
    }
}


status_t AwPlayer::setParameter(int key, const Parcel &request)
{
    logv("setParameter(key=%d)", key);

    switch(key)
    {
        case KEY_PARAMETER_CACHE_STAT_COLLECT_FREQ_MS:
        {
            int nCacheReportIntervalMs;

            logv("setParameter::KEY_PARAMETER_CACHE_STAT_COLLECT_FREQ_MS");

            nCacheReportIntervalMs = request.readInt32();
            if(DemuxCompSetCacheStatReportInterval(mPriData->mDemux, nCacheReportIntervalMs) == 0)
                return OK;
            else
                return UNKNOWN_ERROR;
        }

        case KEY_PARAMETER_PLAYBACK_RATE_PERMILLE:
        {
            logv("setParameter::KEY_PARAMETER_PLAYBACK_RATE_PERMILLE");
            //* TODO.
            return OK;
        }

        default:
        {
            logv("unknown setParameter command %d", key);
            return UNKNOWN_ERROR;
        }
    }
}


status_t AwPlayer::getParameter(int key, Parcel *reply)
{
    logv("getParameter");

    CEDARX_UNUSE(reply);

    switch(key)
    {
        case KEY_PARAMETER_AUDIO_CHANNEL_COUNT:
        {
            logv("getParameter::KEY_PARAMETER_AUDIO_CHANNEL_COUNT");
            //* TODO.
            return OK;
        }
#if (KEY_PARAMETER_GET == 1) //* h3 kk44 need yet.
        case KEY_PARAMETER_GET_CURRENT_BITRATE:
        {
            logd("getParameter::PARAM_KEY_GET_CURRENT_BITRATE");
            if(mPriData->mPlayer != NULL)
            {
                int currentVideoBitrate = 0;
                int currentAudioBitrate = 0;
                int currentBitrate = 0;
                currentVideoBitrate = PlayerGetVideoBitrate(mPriData->mPlayer);
                currentAudioBitrate = PlayerGetAudioBitrate(mPriData->mPlayer);
                currentBitrate = currentVideoBitrate + currentAudioBitrate;
                logd("current Bitrate: video(%d)b/s + audio(%d)b/s = (%d)b/s",
                        currentVideoBitrate, currentAudioBitrate, currentBitrate);
                reply->writeInt32(currentBitrate);
            }
            return OK;
        }
        case KEY_PARAMETER_GET_CURRENT_CACHE_DATA_DURATION:
        {
            logv("getParameter::PARAM_KEY_GET_CURRENT_CACHE_DATA_DURATION");
            if(mPriData->mPlayer != NULL)
            {
                int currentBitrate = 0;
                int currentCacheDataSize = 0;
                int currentCacheDataDuration = 0;
                currentBitrate = PlayerGetVideoBitrate(mPriData->mPlayer) +
                                 PlayerGetAudioBitrate(mPriData->mPlayer);
                currentCacheDataSize = DemuxCompGetCacheSize(mPriData->mDemux);

                if(currentBitrate <= 0)
                {
                    currentCacheDataDuration = 0;
                    reply->writeInt32((int)currentCacheDataDuration);
                }
                else
                {
                    int64_t tmp = (float)currentCacheDataSize*8.0*1000.0;
                    tmp /= (float)currentBitrate;
                    currentCacheDataDuration = (int)tmp;
                    reply->writeInt32((int)currentCacheDataDuration);
                }
                logd("currentDataSize(%d)B currentBitrate(%d)b/s currentDataDuration(%d)ms",
                        currentCacheDataSize, currentBitrate, currentCacheDataDuration);
            }
            return OK;
        }
#endif
        default:
        {
            loge("unknown getParameter command %d", key);
            return UNKNOWN_ERROR;
        }
    }
}


status_t AwPlayer::getMetadata(const media::Metadata::Filter& ids, Parcel *records)
{
    using media::Metadata;

    Metadata metadata(records);

    CEDARX_UNUSE(ids);

    pthread_mutex_lock(&mPriData->mMutexMediaInfo);

    if(mPriData->mStatus == AWPLAYER_STATUS_IDLE ||
       mPriData->mStatus == AWPLAYER_STATUS_INITIALIZED ||
       mPriData->mMediaInfo == NULL)
    {
        pthread_mutex_unlock(&mPriData->mMutexMediaInfo);
        return NO_INIT;
    }

    if(mPriData->mMediaInfo->nDurationMs > 0)
        metadata.appendBool(Metadata::kPauseAvailable , true);
    else
        metadata.appendBool(Metadata::kPauseAvailable , false); //* live stream, can not pause.

    if(mPriData->mMediaInfo->bSeekable)
    {
        metadata.appendBool(Metadata::kSeekBackwardAvailable, true);
        metadata.appendBool(Metadata::kSeekForwardAvailable, true);
        metadata.appendBool(Metadata::kSeekAvailable, true);
    }
    else
    {
        metadata.appendBool(Metadata::kSeekBackwardAvailable, false);
        metadata.appendBool(Metadata::kSeekForwardAvailable, false);
        metadata.appendBool(Metadata::kSeekAvailable, false);
    }

    pthread_mutex_unlock(&mPriData->mMutexMediaInfo);
    return OK;

    //* other metadata in include/media/Metadata.h, Keep in sync with android/media/Metadata.java.
    /*
    Metadata::kTitle;                   // String
    Metadata::kComment;                 // String
    Metadata::kCopyright;               // String
    Metadata::kAlbum;                   // String
    Metadata::kArtist;                  // String
    Metadata::kAuthor;                  // String
    Metadata::kComposer;                // String
    Metadata::kGenre;                   // String
    Metadata::kDate;                    // Date
    Metadata::kDuration;                // Integer(millisec)
    Metadata::kCdTrackNum;              // Integer 1-based
    Metadata::kCdTrackMax;              // Integer
    Metadata::kRating;                  // String
    Metadata::kAlbumArt;                // byte[]
    Metadata::kVideoFrame;              // Bitmap

    Metadata::kBitRate;                 // Integer, Aggregate rate of
    Metadata::kAudioBitRate;            // Integer, bps
    Metadata::kVideoBitRate;            // Integer, bps
    Metadata::kAudioSampleRate;         // Integer, Hz
    Metadata::kVideoframeRate;          // Integer, Hz

    // See RFC2046 and RFC4281.
    Metadata::kMimeType;                // String
    Metadata::kAudioCodec;              // String
    Metadata::kVideoCodec;              // String

    Metadata::kVideoHeight;             // Integer
    Metadata::kVideoWidth;              // Integer
    Metadata::kNumTracks;               // Integer
    Metadata::kDrmCrippled;             // Boolean
    */
}

status_t AwPlayer::updateVideoInfo()
{
    //* get media information.
    MediaInfo*          mi;
    VideoStreamInfo*    vi;
    int                 ret;

    pthread_mutex_lock(&mPriData->mMutexMediaInfo);

    mi = DemuxCompGetMediaInfo(mPriData->mDemux);
    if(mi == NULL)
    {
        loge("can not get media info from demux.");
        pthread_mutex_unlock(&mPriData->mMutexMediaInfo);
        return UNKNOWN_ERROR;
    }
    clearMediaInfo();
    mPriData->mMediaInfo = mi;

#if !AWPLAYER_CONFIG_DISABLE_VIDEO
    if(mi->pVideoStreamInfo != NULL)
    {
        #if(DROP_3D_SECOND_VIDEO_STREAM == 1)
            if(mi->pVideoStreamInfo->bIs3DStream == 1)
                mi->pVideoStreamInfo->bIs3DStream = 0;
        #endif

        ret = PlayerSetVideoStreamInfo(mPriData->mPlayer, mi->pVideoStreamInfo);
        if(ret != 0)
        {
            logw("PlayerSetVideoStreamInfo() fail, video stream not supported.");
        }
    }
#endif

    pthread_mutex_unlock(&mPriData->mMutexMediaInfo);
    return OK;
}

int getKeyFrameNum(const MediaInfo *mi)
{
    int ret = -1;
    if(mi->pVideoStreamInfo->nWidth >= WIDTH_4K || mi->pVideoStreamInfo->nHeight >= HEIGHT_4K)
    {
        /* prob h265 stream reference pictuer number, for 512M DRAM case. */
        if (mi->pVideoStreamInfo->eCodecFormat == VIDEO_CODEC_FORMAT_H265)
        {
            if(mi->pVideoStreamInfo->pCodecSpecificData != NULL &&
                mi->pVideoStreamInfo->nCodecSpecificDataLen != 0)
            {
                ret = DemuxProbeH265RefPictureNumber(mi->pVideoStreamInfo->pCodecSpecificData,
                        mi->pVideoStreamInfo->nCodecSpecificDataLen);
            }
            if(ret != 0)
            {
                logd(" h265 reference picture number: %d", ret);
            }
            else
            {
                logw(" probe h265 reference picture number fail ");
            }
        }
    }

    return ret;
}

//return KB
int getPlatformMeminfo()
{
    int ret         = 0;
    int meminfo     = 0;
    char buf[512]   = {0};
    const char *split_char = " ";
    char *p         = NULL;

    FILE *fp = fopen("/proc/meminfo", "r");
    if(fp == NULL)
    {
        printf("cannot open /proc/meminfo");
        goto _errout;
    }

    fgets(buf, sizeof(buf), fp);

    strtok(buf, split_char);
    while((p=strtok(NULL,split_char)))
    {
        meminfo = atoi(p);
        if(meminfo > 0)
        {
            logd("get meminfo=[%d]\n", meminfo);
            break;
        }
    }

    ret = meminfo;
_errout:
    if(fp != NULL)
    {
        fclose(fp);
    }
    return ret;
}


/*
 *ret 1 means need to scale down
 *    0 is do nothing
 *    -1 is unknown error
 * */
int getScaledownFlag(const MediaInfo *mi)
{
    int ret = -1;
    /*A80              general 4K -> do nothing (0)*/
    /*H3/H8            general 4K -> Scale down (1)*/
    /*H3            H265 4K    -> follow the policy*/
    if(mi->pVideoStreamInfo->nWidth >= WIDTH_4K || mi->pVideoStreamInfo->nHeight >= HEIGHT_4K)
    {
        if (mi->pVideoStreamInfo->eCodecFormat == VIDEO_CODEC_FORMAT_H265)
        {
#if (H265_4K_CHECK_SCALE_DOWN == 1) //H3 and A64 have h265 hardware codec
            int meminfo = getPlatformMeminfo();
            int keyFrameNum = getKeyFrameNum(mi);
            /*
             *get media.hevc.scaldown property
             *if 1 , force scale down.
             *others, do nothing
             * */
            char value[PROP_VALUE_MAX] = {0};
            property_get("media.hevc.scaledown", value, "0");

            /*do nothing for large than 512M*/
            if(meminfo > 512*1024)
            {
                ret = 0;
            }
            else if(strcmp("1", value)==0)
            {
                logd("get media.hevc.scaledown is [%s]", value);
                ret = 1;
            }
            /*keyFrameNum>4 will scaledown, default scaledown. */
            else if(keyFrameNum<=0 || keyFrameNum>4)
            {
                ret = 1;
            }
            else
            {
                logd("support H265 4K P2P.");
                ret = 0;
            }
#endif
        }
        else
        {
#if (NON_H265_4K_NOT_SCALE_DOWN) //A80 do nothing
            ret = 0;
#else    //H3/H8 scale down
            ret = 1;
#endif
        }
    }

    return ret;
}

/*
 *ret 0 means supported, -1 is unsupported.
 * */
int checkVideoSupported(const MediaInfo *mi, const char* strApkName)
{
    CDX_UNUSE(strApkName);
    int ret = 0;    //default is supported


    /*A80/H8          H265 4K unsupported*/
    /*H3              H265 4K supported*/
    if (mi->pVideoStreamInfo->nWidth >= WIDTH_4K || mi->pVideoStreamInfo->nHeight >= HEIGHT_4K)
    {
#if (SUPPORT_H265 == 0) //* ve support h265 on 1680, 4K is ok!
        if (mi->pVideoStreamInfo->eCodecFormat == VIDEO_CODEC_FORMAT_H265)
        {
            loge("Not support H265 4K video !!");
            ret = -1;
        }
#endif

        // [H3 costdown] As DDR frequence limited, it will not support H264 4K decoder.
        if (mi->pVideoStreamInfo->eCodecFormat == VIDEO_CODEC_FORMAT_H264)
        {
            int freq = MemAdapterGetDramFreq();
            if (freq > 0 && freq < 360000)
            {
                loge("Not support H264 4K video, as ddr freq(%d) limited", freq);
                return -1;
            }
            else if(freq < 0)
            {
                logw("unknow ddr freq(%d), it may cause some problem", freq);
            }
        }
    }
    /*H8/H3, WMV1/WMV2/VP6 specs unsupported, but we still support to play, so return supported. */
    /*A80 WMV1/WMV2/VP6 supported */
    else if (mi->pVideoStreamInfo->nWidth >= WIDTH_1080P ||
            mi->pVideoStreamInfo->nHeight >= HEIGHT_1080P)
    {
        if (mi->pVideoStreamInfo->eCodecFormat == VIDEO_CODEC_FORMAT_WMV1
                || mi->pVideoStreamInfo->eCodecFormat == VIDEO_CODEC_FORMAT_WMV2
                || mi->pVideoStreamInfo->eCodecFormat == VIDEO_CODEC_FORMAT_VP6)
        {
            loge("Not support WMV1/WMV2/VP6 1080P video !!");
#if (ANTUTU_NOT_SUPPORT == 1)
            if(!strstr(strApkName, "antutu"))
            {
                ret = -1;
            }
#endif
        }
    }
    else /* < 1080P */
    {
        /* we support this*/
    }

    logv("check video support ret = [%d]", ret);
    return ret;
}

void enableMediaBoost(MediaInfo* mi, int* logLevelOut)
{
    char cmd[PROP_VALUE_MAX] = {0};
    int total = 0;
    struct ScMemOpsS *memops = NULL;
    char buf[10] = {0};
    int loglevel = 4;

	*logLevelOut = loglevel;
    if(mi == NULL || mi->pVideoStreamInfo == NULL)
    {
        logd("input invalid args.");
        return;
    }

#if(ENABLE_MEDIA_BOOST_MEM == 1)

    if(mi->pVideoStreamInfo->bSecureStreamFlagLevel1 == 1)
    {
        memops =  SecureMemAdapterGetOpsS();
    }
    else
    {
        memops =  MemAdapterGetOpsS();
    }
    CdcMemOpen(memops);
    total = CdcMemTotalSize(memops);
    CdcMemClose(memops);

    //set the mem property
    if((mi->pVideoStreamInfo->nWidth >= WIDTH_4K || mi->pVideoStreamInfo->nHeight >= HEIGHT_4K)
      && total < 190000 )
    {
        sprintf(cmd, "model%d:3", getpid());
        logd("setprop media.boost.pref %s", cmd);
        property_set("media.boost.pref", cmd);
    }
#endif

//setprop media.boost.pref mode123:0:c:num
//attention!!! Just for h5, num(1,2,3,4) lock cpu num; num(5) lock freq.
#if(ENABLE_MEDIA_BOOST_CPU == 1)
    sprintf(cmd, "mode%d:0:c:5", getpid());
    logd("setprop media.boost.pref %s", cmd);
    property_set("media.boost.pref", cmd);
#endif

#if(ENABLE_MEDIA_BOOST_UARTLOG == 1)
    FILE *fp = fopen("/proc/sys/kernel/printk", "r");
    if(fp == NULL)
    {
        logd("cannot open /proc/sys/kernel/printk");
        return;
    }
    fgets(buf, sizeof(buf), fp);
    fclose(fp);
    buf[1] = '\0';
    loglevel = atoi(buf);
    if(loglevel > 0)
    {
        *logLevelOut = loglevel;
    }

    fp = fopen("/proc/sysrq-trigger", "w");
    if(fp == NULL)
    {
        logd("cannot open /proc/sysrq-trigger");
        return;
    }
    int ret = fputs("1", fp);
    if(ret == 0)
    {
        logd("origin log level is %d, now set log level to 1", *logLevelOut);
    } else {
        logd("set log level failed!!");
    }
    fclose(fp);
#endif
}

void disableMediaBoost(int logLevel)
{
    char value[PROP_VALUE_MAX] = {0};
    char cmd[PROP_VALUE_MAX] = {0};
    char loglevel[5] = {0};

    property_get("media.boost.pref", value, "0");

#if(ENABLE_MEDIA_BOOST_MEM == 1)
    if(strlen(value)>1 && strncmp(&value[strlen(value)-1], "3", 1)==0)
    {
        sprintf(cmd, "model%d:0", getpid());
        logd("setprop media.boost.pref %s", cmd);
        property_set("media.boost.pref", cmd);
    }
#endif
#if(ENABLE_MEDIA_BOOST_CPU == 1)
    sprintf(cmd, "mode%d:0:c:0", getpid());
    logd("setprop media.boost.pref %s", cmd);
    property_set("media.boost.pref", cmd);
#endif

    FILE *fp = fopen("/proc/sysrq-trigger", "w");
    if(fp == NULL)
    {
        logd("cannot open /proc/sysrq-trigger");
        return;
    }
    sprintf(loglevel, "%d", logLevel);
    int ret = fputs(loglevel, fp);
    if(ret == 0)
    {
        logd("set log level to origin:%d successful!!", logLevel);
    } else {
        logd("set log level to origin:%d failed!!", logLevel);
    }
    fclose(fp);
}

void setMemoryLimit(Player *player)
{
    (void)player;
// TODO: do it in mem implement.
#if 0

    int     limit = 0;
     int     total = 0;

    if(player == NULL)
    {
        logd("input invalid args.");
        goto _errout;
    }

    total = MemAdapterGetTotalMemory(); //return kb
    if(total <= 0)
    {
        logd("cannot get total momery.");
        goto _errout;
    }

    limit = (total-10*1024)*1024;

    logd("set mem limit to [%d].", limit);

    PlayerConfigSetMemoryThresh(player, limit);

_errout:
#endif
    return;
}

status_t AwPlayer::initializePlayer()
{
    //* get media information.
    MediaInfo*          mi;
    VideoStreamInfo*    vi;
    AudioStreamInfo*    ai;
    SubtitleStreamInfo* si;
    int                 i;
    int                 nDefaultAudioIndex;
    int                 nDefaultSubtitleIndex;
    int                 ret;
    int                 bIsSecureStreamFlag = 0;

    pthread_mutex_lock(&mPriData->mMutexMediaInfo);

    mi = DemuxCompGetMediaInfo(mPriData->mDemux);
    if(mi == NULL)
    {
        loge("can not get media info from demux.");
        pthread_mutex_unlock(&mPriData->mMutexMediaInfo);
        return UNKNOWN_ERROR;
    }

    PlayerSetFirstPts(mPriData->mPlayer,mi->nFirstPts);
    mPriData->mMediaInfo = mi;
    if(mi->pVideoStreamInfo != NULL)
       {
        /*detect if support*/
        if(checkVideoSupported(mi, mPriData->strApkName))
        {
            loge("this video is outof specs, unsupported.");
            return UNKNOWN_ERROR;
        }

        /*check if need scaledown*/
        if(getScaledownFlag(mi)==1) /*1 means need to scale down*/
        {
            ret = PlayerConfigVideoScaleDownRatio(mPriData->mPlayer, 1, 1);
            if(ret != 0)
            {
                logw("PlayerConfigVideoScaleDownRatio() fail, ret = %d",ret);
            }
            else
                mPriData->mScaledownFlag = 1;
        }

        setMemoryLimit(mPriData->mPlayer);
       }

    //* initialize the player.
#if !AWPLAYER_CONFIG_DISABLE_VIDEO
    if(mi->pVideoStreamInfo != NULL)
    {
        bIsSecureStreamFlag = mi->pVideoStreamInfo->bSecureStreamFlag;
#if(DROP_3D_SECOND_VIDEO_STREAM == 1)
        if(mi->pVideoStreamInfo->bIs3DStream == 1)
            mi->pVideoStreamInfo->bIs3DStream = 0;
#endif

        if (mi->is_dtv)
        {
            PlayerConfigTvStreamFlag(mPriData->mPlayer, 1);
        }

        //* set the rotation
        int nRotateDegree;
        int nRotation = atoi((const char*)mPriData->mMediaInfo->cRotation);

        if(nRotation == 0)
            nRotateDegree = 0;
        else if(nRotation == 90)
            nRotateDegree = 1;
        else if(nRotation == 180)
            nRotateDegree = 2;
        else if(nRotation == 270)
            nRotateDegree = 3;
        else
            nRotateDegree = 0;

        ret = PlayerConfigVideoRotateDegree(mPriData->mPlayer, nRotateDegree);
        if(ret != 0)
        {
            logw("PlayerConfigVideoRotateDegree() fail, ret = %d",ret);
        }

        //* set the video streamInfo
        ret = PlayerSetVideoStreamInfo(mPriData->mPlayer, mi->pVideoStreamInfo);
        if(ret != 0)
        {
            logw("PlayerSetVideoStreamInfo() fail, video stream not supported.");
            return UNKNOWN_ERROR;
        }
#if 1
        if(DROP_DELAY_FRAME == DROP_DELAY_FRAME_720P)
        {
            if(mi->pVideoStreamInfo!=NULL)
            {
                if(mi->pVideoStreamInfo->nWidth>=1280 && mi->pVideoStreamInfo->nHeight>=720)
                {
                    PlayerConfigDropLaytedFrame(mPriData->mPlayer, 1);
                }
            }
        }
        else if(DROP_DELAY_FRAME == DROP_DELAY_FRAME_4K)
        {
            if(mi->pVideoStreamInfo!=NULL)
            {
                if(mi->pVideoStreamInfo->nWidth>=3840 && mi->pVideoStreamInfo->nHeight>=2160)
                {
                    PlayerConfigDropLaytedFrame(mPriData->mPlayer, 1);
                }
            }
        }
#endif
    }
#endif

#if !AWPLAYER_CONFIG_DISABLE_AUDIO
    if(mi->pAudioStreamInfo != NULL)
    {
        nDefaultAudioIndex = -1;
        for(i=0; i<mi->nAudioStreamNum; i++)
        {
            if(PlayerCanSupportAudioStream(mPriData->mPlayer, &mi->pAudioStreamInfo[i]))
            {
                nDefaultAudioIndex = i;
                break;
            }
        }

        if(nDefaultAudioIndex < 0)
        {
            logw("no audio stream supported.");
            nDefaultAudioIndex = 0;
        }

        ret = PlayerSetAudioStreamInfo(mPriData->mPlayer, mi->pAudioStreamInfo,
                                       mi->nAudioStreamNum, nDefaultAudioIndex);
        if(ret != 0)
        {
            logw("PlayerSetAudioStreamInfo() fail, audio stream not supported.");
        }

        #if(MUTE_DRM_WHEN_HDMI_FLAG)
        if(mPriData->mHDMIListener && bIsSecureStreamFlag) {
            /*only  start thread if media is encrypted*/
            mPriData->mHDMIListener->setNotifyCallback(this, HDMINotify);
            mPriData->mHDMIListener->start();
        }
        #endif
    }
#endif

    if(PlayerHasVideo(mPriData->mPlayer) == 0 && PlayerHasAudio(mPriData->mPlayer) == 0)
    {
        loge("neither video nor audio stream can be played.");
        pthread_mutex_unlock(&mPriData->mMutexMediaInfo);
        return UNKNOWN_ERROR;
    }

#if !AWPLAYER_CONFIG_DISABLE_SUBTITLE
    //* set subtitle stream to the text decoder.
    if(mi->pSubtitleStreamInfo != NULL)
    {
        nDefaultSubtitleIndex = -1;
        for(i=0; i<mi->nSubtitleStreamNum; i++)
        {
            if(PlayerCanSupportSubtitleStream(mPriData->mPlayer, &mi->pSubtitleStreamInfo[i]))
            {
                nDefaultSubtitleIndex = i;
                break;
            }
        }

        if(nDefaultSubtitleIndex < 0)
        {
            logw("no subtitle stream supported.");
            nDefaultSubtitleIndex = 0;
        }

        ret = PlayerSetSubtitleStreamInfo(mPriData->mPlayer, mi->pSubtitleStreamInfo,
                                          mi->nSubtitleStreamNum, nDefaultSubtitleIndex);
        if(ret != 0)
        {
            logw("PlayerSetSubtitleStreamInfo() fail, subtitle stream not supported.");
        }
    }
#endif

    //* report not seekable.
    if(mi->bSeekable == 0)
        sendEvent(MEDIA_INFO, MEDIA_INFO_NOT_SEEKABLE, 0);

    pthread_mutex_unlock(&mPriData->mMutexMediaInfo);
    return OK;
}


void AwPlayer::clearMediaInfo()
{
    int                 i;
    VideoStreamInfo*    v;
    AudioStreamInfo*    a;
    SubtitleStreamInfo* s;

    if(mPriData->mMediaInfo != NULL)
    {
        //* free video stream info.
        if(mPriData->mMediaInfo->pVideoStreamInfo != NULL)
        {
            for(i=0; i<mPriData->mMediaInfo->nVideoStreamNum; i++)
            {
                v = &mPriData->mMediaInfo->pVideoStreamInfo[i];
                if(v->pCodecSpecificData != NULL && v->nCodecSpecificDataLen > 0)
                    free(v->pCodecSpecificData);
            }
            free(mPriData->mMediaInfo->pVideoStreamInfo);
            mPriData->mMediaInfo->pVideoStreamInfo = NULL;
        }

        //* free audio stream info.
        if(mPriData->mMediaInfo->pAudioStreamInfo != NULL)
        {
            for(i=0; i<mPriData->mMediaInfo->nAudioStreamNum; i++)
            {
                a = &mPriData->mMediaInfo->pAudioStreamInfo[i];
                if(a->pCodecSpecificData != NULL && a->nCodecSpecificDataLen > 0)
                    free(a->pCodecSpecificData);
            }
            free(mPriData->mMediaInfo->pAudioStreamInfo);
            mPriData->mMediaInfo->pAudioStreamInfo = NULL;
        }

        //* free subtitle stream info.
        if(mPriData->mMediaInfo->pSubtitleStreamInfo != NULL)
        {
            for(i=0; i<mPriData->mMediaInfo->nSubtitleStreamNum; i++)
            {
                s = &mPriData->mMediaInfo->pSubtitleStreamInfo[i];
                if(s->pUrl != NULL)
                {
                    free(s->pUrl);
                    s->pUrl = NULL;
                }
                if(s->fd >= 0)
                {
                    ::close(s->fd);
                    s->fd = -1;
                }
                if(s->fdSub >= 0)
                {
                    ::close(s->fdSub);
                    s->fdSub = -1;
                }
            }
            free(mPriData->mMediaInfo->pSubtitleStreamInfo);
            mPriData->mMediaInfo->pSubtitleStreamInfo = NULL;
        }

        //* free the media info.
        free(mPriData->mMediaInfo);
        mPriData->mMediaInfo = NULL;
    }

    return;
}


status_t AwPlayer::mainThread()
{
    AwMessage            msg;
    int                  ret;
    sem_t*               pReplySem;
    int*                 pReplyValue;

    while(1)
    {
        if(AwMessageQueueGetMessage(mPriData->mMessageQueue, &msg) < 0)
        {
            loge("get message fail.");
            continue;
        }

        pReplySem   = (sem_t*)msg.params[0];
        pReplyValue = (int*)msg.params[1];

        if(msg.messageId == AWPLAYER_COMMAND_SET_SOURCE)
        {
            logi("process message AWPLAYER_COMMAND_SET_SOURCE.");
            //* check status.
            if(mPriData->mStatus != AWPLAYER_STATUS_IDLE &&
                    mPriData->mStatus != AWPLAYER_STATUS_INITIALIZED)
            {
                loge("invalid setDataSource() operation, player not in IDLE or INITIALIZED status");
                if(pReplyValue != NULL)
                    *pReplyValue = (int)INVALID_OPERATION;
                if(pReplySem != NULL)
                    sem_post(pReplySem);
                continue;
            }

            if((int)msg.params[2] == SOURCE_TYPE_URL)
            {
                KeyedVector<String8, String8>* pHeaders;
                //* data source is a url string.
                if(mPriData->mSourceUrl != NULL)
                    free(mPriData->mSourceUrl);
                mPriData->mSourceUrl = strdup((char*)msg.params[3]);
                pHeaders   = (KeyedVector<String8, String8>*) msg.params[4];

#if (CONFIG_OS_VERSION >= OPTION_OS_VERSION_ANDROID_5_0)
                ret = DemuxCompSetUrlSource(mPriData->mDemux, (void*)msg.params[5],
                                            mPriData->mSourceUrl, pHeaders);
#else
                ret = DemuxCompSetUrlSource(mPriData->mDemux, mPriData->mSourceUrl, pHeaders);
#endif
                if(ret == 0)
                {
                    mPriData->mStatus = AWPLAYER_STATUS_INITIALIZED;
                    if(pReplyValue != NULL)
                        *pReplyValue = (int)OK;
                }
                else
                {
                    loge("DemuxCompSetUrlSource() return fail.");
                    mPriData->mStatus = AWPLAYER_STATUS_IDLE;
                    free(mPriData->mSourceUrl);
                    mPriData->mSourceUrl = NULL;
                    if(pReplyValue != NULL)
                        *pReplyValue = (int)UNKNOWN_ERROR;
                }
            }
            else if((int)msg.params[2] == SOURCE_TYPE_FD)
            {
                //* data source is a file descriptor.
                int     fd;
                int64_t nOffset;
                int64_t nLength;

                fd = msg.params[3];
                nOffset = msg.params[4];
                nOffset<<=32;
                nOffset |= msg.params[5];
                nLength = msg.params[6];
                nLength<<=32;
                nLength |= msg.params[7];

                if(mPriData->mSourceFd != -1)
                    ::close(mPriData->mSourceFd);

                mPriData->mSourceFd       = dup(fd);
                mPriData->mSourceFdOffset = nOffset;
                mPriData->mSourceFdLength = nLength;
                ret = DemuxCompSetFdSource(mPriData->mDemux, mPriData->mSourceFd,
                            mPriData->mSourceFdOffset, mPriData->mSourceFdLength);
                if(ret == 0)
                {
                    mPriData->mStatus = AWPLAYER_STATUS_INITIALIZED;
                    if(pReplyValue != NULL)
                        *pReplyValue = (int)OK;
                }
                else
                {
                    loge("DemuxCompSetFdSource() return fail.");
                    mPriData->mStatus = AWPLAYER_STATUS_IDLE;
                    ::close(mPriData->mSourceFd);
                    mPriData->mSourceFd = -1;
                    mPriData->mSourceFdOffset = 0;
                    mPriData->mSourceFdLength = 0;
                    if(pReplyValue != NULL)
                        *pReplyValue = (int)UNKNOWN_ERROR;
                }
            }
            else
            {
                //* data source is a IStreamSource interface.
                char *uri = (char *)msg.params[3];
                int ret;
                void *handle;
                ret = sscanf(uri, "customer://%p", &handle);
                if (ret != 1)
                {
                    CDX_LOGE("sscanf failure...(%s)", uri);
                    mPriData->mSourceStream = NULL;
                }
                else
                {
                    mPriData->mSourceStream = (CdxStreamT *)handle;
                }
                ret = DemuxCompSetStreamSource(mPriData->mDemux, uri);
                if(ret == 0)
                {
                    mPriData->mStatus = AWPLAYER_STATUS_INITIALIZED;
                    if(pReplyValue != NULL)
                        *pReplyValue = (int)OK;
                }
                else
                {
                    loge("DemuxCompSetStreamSource() return fail.");
                    mPriData->mStatus = AWPLAYER_STATUS_IDLE;
                    if(mPriData->mSourceStream)
                    {
                        CdxStreamClose(mPriData->mSourceStream);
                        mPriData->mSourceStream = NULL;
                    }
                    if(pReplyValue != NULL)
                        *pReplyValue = (int)UNKNOWN_ERROR;
                }
            }

            if(pReplySem != NULL)
                sem_post(pReplySem);
            continue;
        } //* end AWPLAYER_COMMAND_SET_SOURCE.
        else if(msg.messageId == AWPLAYER_COMMAND_SET_SURFACE)
        {
#if CONFIG_OS_VERSION != OPTION_OS_VERSION_ANDROID_4_2
            sp<IGraphicBufferProducer> graphicBufferProducer;
#else
            sp<ISurfaceTexture> surfaceTexture;
#endif
            sp<ANativeWindow>   anw;

            logv("process message AWPLAYER_COMMAND_SET_SURFACE.");

            //* set native window before delete the old one.
            //* because the player's render thread may use the old surface
            //* before it receive the new surface.
#if CONFIG_OS_VERSION != OPTION_OS_VERSION_ANDROID_4_2
            graphicBufferProducer = (IGraphicBufferProducer *)msg.params[2];
            if(graphicBufferProducer.get() != NULL)
                anw = new Surface(graphicBufferProducer);
#else
            surfaceTexture = (ISurfaceTexture*)msg.params[2];
            if(surfaceTexture.get() != NULL)
                anw = new SurfaceTextureClient(surfaceTexture);
#endif
            else
                anw = NULL;

            ret = PlayerSetWindow(mPriData->mPlayer, anw.get());

            //* save the new surface.
#if CONFIG_OS_VERSION != OPTION_OS_VERSION_ANDROID_4_2
            mPriData->mGraphicBufferProducer = graphicBufferProducer;
#else
            mPriData->mSurfaceTexture = surfaceTexture;
#endif
            if(mPriData->mNativeWindow != NULL)
                native_window_api_disconnect(mPriData->mNativeWindow.get(),
                                             NATIVE_WINDOW_API_MEDIA);
            mPriData->mNativeWindow   = anw;

            if(ret == 0)
            {
                if(pReplyValue != NULL)
                    *pReplyValue = (int)OK;
            }
            else
            {
                loge("PlayerSetWindow() return fail.");
                if(pReplyValue != NULL)
                    *pReplyValue = (int)UNKNOWN_ERROR;
            }

            if(pReplySem != NULL)
                sem_post(pReplySem);
            continue;
        } //* end AWPLAYER_COMMAND_SET_SURFACE.
        else if(msg.messageId == AWPLAYER_COMMAND_SET_AUDIOSINK)
        {
            sp<AudioSink> audioSink;

            logv("process message AWPLAYER_COMMAND_SET_AUDIOSINK.");

            audioSink = (AudioSink*)msg.params[2];
            PlayerSetAudioSink(mPriData->mPlayer, audioSink.get());

            //* save the new surface.
            MediaPlayerInterface::setAudioSink(audioSink);

            if(pReplyValue != NULL)
                *pReplyValue = (int)OK;
            if(pReplySem != NULL)
                sem_post(pReplySem);
            continue;
        } //* end AWPLAYER_COMMAND_SET_AUDIOSINK.
        else if(msg.messageId == AWPLAYER_COMMAND_PREPARE)
        {
            logv("process message AWPLAYER_COMMAND_PREPARE.");

            if(mPriData->mStatus == AWPLAYER_STATUS_PREPARED)
            {
                //* for data source set by fd(file descriptor), the prepare() method
                //* is called in setDataSource(), so the player is already in PREPARED
                //* status, here we just notify a prepared message.

                //* when app call prepareAsync(), we callback video-size to app here,
                if(mPriData->mMediaInfo->pVideoStreamInfo != NULL)
                {
                    int width = mPriData->mMediaInfo->pVideoStreamInfo->nWidth;
                    int height = mPriData->mMediaInfo->pVideoStreamInfo->nHeight;
                    if(width != mPriData->mVideoSizeWidth &&
                            height != mPriData->mVideoSizeHeight)
                    {
                        int nRotation;

                        nRotation = atoi((const char*)mPriData->mMediaInfo->cRotation);
                        if((nRotation%180)==0)//* when the rotation is 0 and 180
                        {
                            mPriData->mVideoSizeWidth  = width;
                            mPriData->mVideoSizeHeight = height;
                        }
                        else
                        {
                            mPriData->mVideoSizeWidth  = height;
                            mPriData->mVideoSizeHeight = width;
                        }

                        logi("xxxxxxxxxx video size: width = %d, height = %d",
                                mPriData->mVideoSizeWidth, mPriData->mVideoSizeHeight);
                        sendEvent(MEDIA_SET_VIDEO_SIZE,
                                mPriData->mVideoSizeWidth, mPriData->mVideoSizeHeight);
                    }
                }

                sendEvent(MEDIA_PREPARED, 0, 0);
                if(pReplyValue != NULL)
                    *pReplyValue = (int)OK;
                if(pReplySem != NULL)
                    sem_post(pReplySem);
                continue;
            }

            if(mPriData->mStatus != AWPLAYER_STATUS_INITIALIZED &&
                    mPriData->mStatus != AWPLAYER_STATUS_STOPPED)
            {
                logd("invalid prepareAsync() call, player not in initialized or stopped status.");
                if(pReplyValue != NULL)
                    *pReplyValue = (int)INVALID_OPERATION;
                if(pReplySem != NULL)
                    sem_post(pReplySem);
                continue;
            }

            mPriData->mStatus = AWPLAYER_STATUS_PREPARING;
            mPriData->mPrepareSync = msg.params[2];
            ret = DemuxCompPrepareAsync(mPriData->mDemux);
            if(ret != 0)
            {
                loge("DemuxCompPrepareAsync return fail immediately.");
                mPriData->mStatus = AWPLAYER_STATUS_IDLE;
                if(pReplyValue != NULL)
                    *pReplyValue = (int)INVALID_OPERATION;
            }
            else
            {
                if(pReplyValue != NULL)
                    *pReplyValue = (int)OK;
            }

            if(pReplySem != NULL)
                sem_post(pReplySem);
            continue;
        } //* end AWPLAYER_COMMAND_PREPARE.
        else if(msg.messageId == AWPLAYER_COMMAND_START)
        {
            logv("process message AWPLAYER_COMMAND_START.");
            if(mPriData->mStatus != AWPLAYER_STATUS_PREPARED &&
               mPriData->mStatus != AWPLAYER_STATUS_STARTED  &&
               mPriData->mStatus != AWPLAYER_STATUS_PAUSED   &&
               mPriData->mStatus != AWPLAYER_STATUS_COMPLETE)
            {
                logd("invalid start() call, player not in prepared, started, "
                        "paused or complete status.");
                if(pReplyValue != NULL)
                    *pReplyValue = (int)INVALID_OPERATION;
                if(pReplySem != NULL)
                    sem_post(pReplySem);
                continue;
            }

            //* synchronize with the seek or complete callback and the status may be changed.
            pthread_mutex_lock(&mPriData->mMutexStatus);

            if(mPriData->mStatus == AWPLAYER_STATUS_STARTED)
            {
                if(PlayerGetStatus(mPriData->mPlayer) == PLAYER_STATUS_PAUSED &&
                        mPriData->mSeeking == 0)
                {
                    //* player is paused for buffering, start it.
                    //* see AWPLAYER_MESSAGE_DEMUX_PAUSE_PLAYER callback message.
                    PlayerStart(mPriData->mPlayer);
                    DemuxCompStart(mPriData->mDemux);
                }
                pthread_mutex_unlock(&mPriData->mMutexStatus);
                logv("player already in started status.");
                if(pReplyValue != NULL)
                    *pReplyValue = (int)OK;
                if(pReplySem != NULL)
                    sem_post(pReplySem);
                continue;
            }

            if(mPriData->mSeeking)
            {
                //* player and demux will be started at the seek callback.
                mPriData->mStatus = AWPLAYER_STATUS_STARTED;
                pthread_mutex_unlock(&mPriData->mMutexStatus);

                if(pReplyValue != NULL)
                    *pReplyValue = (int)OK;
                if(pReplySem != NULL)
                    sem_post(pReplySem);
                continue;
            }

            //* for complete status, we seek to the begin of the file.
            if(mPriData->mStatus == AWPLAYER_STATUS_COMPLETE)
            {
                AwMessage newMsg;

                if(mPriData->mMediaInfo->bSeekable)
                {
                    setMessage(&newMsg,
                               //* message id.
                               AWPLAYER_COMMAND_SEEK,
                               //* params[0] = &mSemSeek, internal message, do not post.
                               0,
                               //* params[1] = &mSeekReply, internal message, do not set reply.
                               0,
                               //* params[2] = mSeekTime(ms).
                               mPriData->mSeekTime,
                               //* params[3] = mSeekSync.
                               1);
                    AwMessageQueuePostMessage(mPriData->mMessageQueue, &newMsg);

                    mPriData->mStatus = AWPLAYER_STATUS_STARTED;
                    pthread_mutex_unlock(&mPriData->mMutexStatus);
                    if(pReplyValue != NULL)
                        *pReplyValue = (int)OK;
                    if(pReplySem != NULL)
                        sem_post(pReplySem);
                    continue;
                }
                else
                {
                    //* post a stop message.
                    setMessage(&newMsg,
                               //* message id.
                               AWPLAYER_COMMAND_STOP,
                               //* params[0] = &mSemStop, internal message, do not post.
                               0,
                               //* params[1] = &mStopReply, internal message, do not reply.
                               0);
                    AwMessageQueuePostMessage(mPriData->mMessageQueue, &newMsg);

                    //* post a prepare message.
                    setMessage(&newMsg,
                               //* message id.
                               AWPLAYER_COMMAND_PREPARE,
                               //* params[0] = &mSemPrepare, internal message, do not post.
                               0,
                               //* params[1] = &mPrepareReply, internal message, do not reply.
                               0,
                               //* params[2] = mPrepareSync.
                               1);
                    AwMessageQueuePostMessage(mPriData->mMessageQueue, &newMsg);

                    //* post a start message.
                    setMessage(&newMsg,
                               //* message id.
                               AWPLAYER_COMMAND_START,
                               //* params[0] = &mSemStart, internal message, do not post.
                               0,
                               //* params[1] = &mStartReply, internal message, do not reply.
                               0);
                    AwMessageQueuePostMessage(mPriData->mMessageQueue, &newMsg);

                    //* should I reply OK to the user at this moment?
                    //* or just set the semaphore and reply variable to the start message to
                    //* make it reply when start message done?
                    pthread_mutex_unlock(&mPriData->mMutexStatus);
                    if(pReplyValue != NULL)
                        *pReplyValue = (int)OK;
                    if(pReplySem != NULL)
                        sem_post(pReplySem);
                    continue;
                }
            }

            pthread_mutex_unlock(&mPriData->mMutexStatus);

            if(mPriData->mApplicationType == MIRACAST)
            {
                PlayerFast(mPriData->mPlayer, 0);
            }
            else
            {
                PlayerStart(mPriData->mPlayer);
            }

            DemuxCompStart(mPriData->mDemux);
            mPriData->mStatus = AWPLAYER_STATUS_STARTED;
            if(pReplyValue != NULL)
                *pReplyValue = (int)OK;
            if(pReplySem != NULL)
                sem_post(pReplySem);
            continue;

        } //* end AWPLAYER_COMMAND_START.
        else if(msg.messageId == AWPLAYER_COMMAND_STOP)
        {
            logv("process message AWPLAYER_COMMAND_STOP.");
            if(mPriData->mStatus != AWPLAYER_STATUS_PREPARED &&
               mPriData->mStatus != AWPLAYER_STATUS_STARTED  &&
               mPriData->mStatus != AWPLAYER_STATUS_PAUSED   &&
               mPriData->mStatus != AWPLAYER_STATUS_COMPLETE &&
               mPriData->mStatus != AWPLAYER_STATUS_STOPPED)
            {
                logd("invalid stop() call, player not in prepared, paused, "
                        "started, stopped or complete status.");
                if(pReplyValue != NULL)
                    *pReplyValue = (int)INVALID_OPERATION;
                if(pReplySem != NULL)
                    sem_post(pReplySem);
                continue;
            }

            if(mPriData->mStatus == AWPLAYER_STATUS_STOPPED)
            {
                logv("player already in stopped status.");
                if(pReplyValue != NULL)
                    *pReplyValue = (int)OK;
                if(pReplySem != NULL)
                    sem_post(pReplySem);
                continue;
            }

            /* the prepare callback may happen at this moment, so the mStatus
             * may be changed to PREPARED asynchronizely.
             */
            if(mPriData->mStatus == AWPLAYER_STATUS_PREPARING)
            {
                logw("stop() called at preparing status, cancel demux prepare.");
                DemuxCompCancelPrepare(mPriData->mDemux);
            }

            if(mPriData->mSeeking)
            {
                DemuxCompCancelSeek(mPriData->mDemux);
                mPriData->mSeeking = 0;
            }

            DemuxCompStop(mPriData->mDemux);
            PlayerStop(mPriData->mPlayer);
            //* clear all media information in player.
            PlayerClear(mPriData->mPlayer);
            //*clear the mSubtitleDisplayIds
            memset(mPriData->mSubtitleDisplayIds,0xff,64*sizeof(unsigned int));
            mPriData->mSubtitleDisplayIdsUpdateIndex = 0;

            mPriData->mStatus  = AWPLAYER_STATUS_STOPPED;
            if(pReplyValue != NULL)
                *pReplyValue = (int)OK;
            if(pReplySem != NULL)
                sem_post(pReplySem);
            continue;
        } //* end AWPLAYER_COMMAND_STOP.
        else if(msg.messageId == AWPLAYER_COMMAND_PAUSE)
        {
            logv("process message AWPLAYER_COMMAND_PAUSE.");
            if(mPriData->mStatus != AWPLAYER_STATUS_STARTED  &&
               mPriData->mStatus != AWPLAYER_STATUS_PAUSED   &&
               mPriData->mStatus != AWPLAYER_STATUS_COMPLETE)
            {
                logd("invalid pause() call, player not in started, paused or complete status.");
                if(pReplyValue != NULL)
                    *pReplyValue = (int)INVALID_OPERATION;
                if(pReplySem != NULL)
                    sem_post(pReplySem);
                continue;
            }

            if(mPriData->mStatus == AWPLAYER_STATUS_PAUSED)
            {
                logv("player already in paused or complete status.");
                if(pReplyValue != NULL)
                    *pReplyValue = (int)OK;
                if(pReplySem != NULL)
                    sem_post(pReplySem);
                continue;
            }

            //* sync with the seek, complete or pause_player/resume_player call back.
            pthread_mutex_lock(&mPriData->mMutexStatus);

            if(mPriData->mSeeking)
            {
                //* player and demux will be paused at the seek callback.
                mPriData->mStatus = AWPLAYER_STATUS_PAUSED;
                pthread_mutex_unlock(&mPriData->mMutexStatus);

                if(pReplyValue != NULL)
                    *pReplyValue = (int)OK;
                if(pReplySem != NULL)
                    sem_post(pReplySem);
                continue;
            }

            PlayerPause(mPriData->mPlayer);
            mPriData->mStatus = AWPLAYER_STATUS_PAUSED;

            //* sync with the seek, complete or pause_player/resume_player call back.
            pthread_mutex_unlock(&mPriData->mMutexStatus);

            if(pReplyValue != NULL)
                *pReplyValue = (int)OK;
            if(pReplySem != NULL)
                sem_post(pReplySem);
            continue;
        } //* end AWPLAYER_COMMAND_PAUSE.
        else if(msg.messageId == AWPLAYER_COMMAND_RESET)
        {
            logv("process message AWPLAYER_COMMAND_RESET.");
            /* the prepare callback may happen at this moment, so the
             * mStatus may be changed to PREPARED asynchronizely.
             */
            if(mPriData->mStatus == AWPLAYER_STATUS_PREPARING)
            {
                logw("reset() called at preparing status, cancel demux prepare.");
                DemuxCompCancelPrepare(mPriData->mDemux);
            }

            if(mPriData->mSeeking)
            {
                DemuxCompCancelSeek(mPriData->mDemux);
                mPriData->mSeeking = 0;
            }

            //* this will stop the seeking if demux is currently processing seeking message.
            DemuxCompStop(mPriData->mDemux);
            //* it will clear the data source keep inside, this is important for the IStreamSource.
            DemuxCompClear(mPriData->mDemux);

            //* stop and clear the player.
            PlayerStop(mPriData->mPlayer);
            //* it will clear media info config to the player.
            PlayerClear(mPriData->mPlayer);

            //* clear suface.
            if(mPriData->mKeepLastFrame == 0)
            {
                if(mPriData->mNativeWindow != NULL)
                    native_window_api_disconnect(mPriData->mNativeWindow.get(),
                            NATIVE_WINDOW_API_MEDIA);
                mPriData->mNativeWindow.clear();
#if CONFIG_OS_VERSION != OPTION_OS_VERSION_ANDROID_4_2
                mPriData->mGraphicBufferProducer.clear();
#else
                mPriData->mSurfaceTexture.clear();
#endif
            }

            //* clear audio sink.
            mAudioSink.clear();

            //* clear data source.
            if(mPriData->mSourceUrl != NULL)
            {
                free(mPriData->mSourceUrl);
                mPriData->mSourceUrl = NULL;
            }
            if(mPriData->mSourceFd != -1)
            {
                ::close(mPriData->mSourceFd);
                mPriData->mSourceFd = -1;
                mPriData->mSourceFdOffset = 0;
                mPriData->mSourceFdLength = 0;
            }
            mPriData->mSourceStream = NULL;

            //* clear media info.
            clearMediaInfo();

            //* clear loop setting.
            mPriData->mLoop   = 0;

            //* clear the mSubtitleDisplayIds
            memset(mPriData->mSubtitleDisplayIds,0xff,64*sizeof(unsigned int));
            mPriData->mSubtitleDisplayIdsUpdateIndex = 0;

            //* set status to IDLE.
            mPriData->mStatus = AWPLAYER_STATUS_IDLE;

            if(pReplyValue != NULL)
                *pReplyValue = (int)OK;
            if(pReplySem != NULL)
                sem_post(pReplySem);
            continue;
        }
        else if(msg.messageId == AWPLAYER_COMMAND_SEEK)
        {
            logv("process message AWPLAYER_COMMAND_SEEK.");
            if(mPriData->mStatus != AWPLAYER_STATUS_PREPARED &&
               mPriData->mStatus != AWPLAYER_STATUS_STARTED  &&
               mPriData->mStatus != AWPLAYER_STATUS_PAUSED   &&
               mPriData->mStatus != AWPLAYER_STATUS_COMPLETE)
            {
                logd("invalid seekTo() call, player not in prepared, "
                        "started, paused or complete status.");
                if(pReplyValue != NULL)
                    *pReplyValue = (int)INVALID_OPERATION;
                if(pReplySem != NULL)
                    sem_post(pReplySem);
                continue;
            }

            //* the application will call seekTo() when player is in complete status.
            //* after seekTo(), the player should still stay on complete status until
            //* application call start().
            //* cts test requires this implement.
            if(mPriData->mStatus == AWPLAYER_STATUS_COMPLETE)
            {
                //* protect mSeeking and mSeekTime from being changed by the seek finish callback.
                pthread_mutex_lock(&mPriData->mMutexStatus);
                mPriData->mSeekTime = msg.params[2];
                pthread_mutex_unlock(&mPriData->mMutexStatus);
                sendEvent(MEDIA_SEEK_COMPLETE, 0, 0);
                if(pReplyValue != NULL)
                    *pReplyValue = (int)0;
                if(pReplySem != NULL)
                    sem_post(pReplySem);
                continue;
            }

            if(mPriData->mMediaInfo == NULL || mPriData->mMediaInfo->bSeekable == 0)
            {
                if(mPriData->mMediaInfo == NULL)
                {
                    loge("seekTo fail because mMediaInfo == NULL.");
                    sendEvent(MEDIA_SEEK_COMPLETE, 0, 0);
                    if(pReplyValue != NULL)
                        *pReplyValue = (int)NO_INIT;
                    if(pReplySem != NULL)
                        sem_post(pReplySem);
                    continue;
                }
                else
                {
                    loge("media not seekable.");
                    sendEvent(MEDIA_SEEK_COMPLETE, 0, 0);
                    if(pReplyValue != NULL)
                        *pReplyValue = 0;
                    if(pReplySem != NULL)
                        sem_post(pReplySem);
                    continue;
                }
            }

            if(mPriData->mSeeking)
            {
                DemuxCompCancelSeek(mPriData->mDemux);
                mPriData->mSeeking = 0;
            }

            //* protect mSeeking and mSeekTime from being changed by the seek finish callback.
            pthread_mutex_lock(&mPriData->mMutexStatus);
            mPriData->mSeeking  = 1;
            mPriData->mSeekTime = msg.params[2];
            mPriData->mSeekSync = msg.params[3];
            logv("seekTo %.2f secs", mPriData->mSeekTime / 1E3);
            pthread_mutex_unlock(&mPriData->mMutexStatus);

            if(PlayerGetStatus(mPriData->mPlayer) == PLAYER_STATUS_STOPPED)
            {
                //* if in prepared status, the player is in stopped status,
                //* this will make the player not record the nSeekTime at PlayerReset() operation
                //* called at seek finish callback.
                PlayerStart(mPriData->mPlayer);
            }
            PlayerPause(mPriData->mPlayer);
            DemuxCompSeekTo(mPriData->mDemux, mPriData->mSeekTime);

            if(pReplyValue != NULL)
                *pReplyValue = (int)OK;
            if(pReplySem != NULL)
                sem_post(pReplySem);
            continue;
        } //* end AWPLAYER_COMMAND_SEEK.
        else if(msg.messageId == AWPLAYER_COMMAND_QUIT)
        {
            if(pReplyValue != NULL)
                *pReplyValue = (int)OK;
            if(pReplySem != NULL)
                sem_post(pReplySem);
            break;  //* break the thread.
        } //* end AWPLAYER_COMMAND_QUIT.
        else
        {
            logw("unknow message with id %d, ignore.", msg.messageId);
        }
    }

    return OK;
}

status_t AwPlayer::setSubCharset(const char* strFormat)
{
    if(strFormat != NULL)
    {
        int i;
        for(i=0; strTextCodecFormats[i]; i++)
        {
            if(!strcmp(strTextCodecFormats[i], strFormat))
                break;
        }

        if(strTextCodecFormats[i] != NULL)
            strcpy(mPriData->mDefaultTextFormat, strTextCodecFormats[i]);
    }
    else
        strcpy(mPriData->mDefaultTextFormat, "UTF-8");

    return OK;
}

status_t AwPlayer::getSubCharset(char *charset)
{
    if(mPriData->mPlayer == NULL)
    {
        return -1;
    }

    strcpy(charset, mPriData->mDefaultTextFormat);

    return OK;
}

status_t AwPlayer::setSubDelay(int nTimeMs)
{
    if(mPriData->mPlayer!=NULL)
        return PlayerSetSubtitleShowTimeAdjustment(mPriData->mPlayer, nTimeMs);
    else
        return -1;
}

int AwPlayer::getSubDelay()
{
    if(mPriData->mPlayer!=NULL)
        return PlayerGetSubtitleShowTimeAdjustment(mPriData->mPlayer);
    else
        return -1;
}

status_t AwPlayer::callbackProcess(int messageId, void* param)
{
    switch(messageId)
    {
        case AWPLAYER_MESSAGE_DEMUX_PREPARED:
        {
            uintptr_t tmpPtr = (uintptr_t)param;
            int err = tmpPtr;
            if(err != 0)
            {
                //* demux prepare return fail.
                //* notify a media error event.
                mPriData->mStatus = AWPLAYER_STATUS_ERROR;
                if(mPriData->mPrepareSync == 0)
                {
                    if(err == DEMUX_ERROR_IO)
                        sendEvent(MEDIA_ERROR, MEDIA_ERROR_IO, 0);
                    else
                        sendEvent(MEDIA_ERROR, DEMUX_ERROR_UNKNOWN, 0);
                }
                else
                {
                    if(err == DEMUX_ERROR_IO)
                        mPriData->mPrepareFinishResult = MEDIA_ERROR_IO;
                    else
                        mPriData->mPrepareFinishResult = UNKNOWN_ERROR;
                    sem_post(&mPriData->mSemPrepareFinish);
                }
            }
            else
            {
                //* demux prepare success, initialize the player.
                if(initializePlayer() == OK)
                {
                    //* initialize player success, notify a prepared event.
                    mPriData->mStatus = AWPLAYER_STATUS_PREPARED;
                    if(mPriData->mPrepareSync == 0)
                    {

                        if(mPriData->mMediaInfo->pVideoStreamInfo!=NULL)
                        {
                            int nWidth = mPriData->mMediaInfo->pVideoStreamInfo->nWidth;
                            int nHeight = mPriData->mMediaInfo->pVideoStreamInfo->nHeight;
                            if(nWidth != mPriData->mVideoSizeWidth &&
                                    nHeight != mPriData->mVideoSizeHeight)
                            {
                                int nRotation;

                                nRotation = atoi((const char*)mPriData->mMediaInfo->cRotation);
                                if((nRotation%180)==0)//* when the rotation is 0 and 180
                                {
                                    mPriData->mVideoSizeWidth  = nWidth;
                                    mPriData->mVideoSizeHeight = nHeight;
                                }
                                else
                                {
                                    mPriData->mVideoSizeWidth  = nHeight;
                                    mPriData->mVideoSizeHeight = nWidth;
                                }
                                logi("video size: width = %d, height = %d",
                                        mPriData->mVideoSizeWidth, mPriData->mVideoSizeHeight);
                                sendEvent(MEDIA_SET_VIDEO_SIZE, mPriData->mVideoSizeWidth,
                                        mPriData->mVideoSizeHeight);
                            }
                            else
                            {
                                //此时mVideoSizeWidth必是0
                                logi("video size: width = 0, height = 0");
                                sendEvent(MEDIA_SET_VIDEO_SIZE, 0, 0);
                            }
                        }
                        else
                        {
                            //此时mVideoSizeWidth必是0
                            logi("video size: width = 0, height = 0");
                            sendEvent(MEDIA_SET_VIDEO_SIZE, 0, 0);
                        }

                        sendEvent(MEDIA_PREPARED, 0, 0);
                    }
                    else
                    {
                        mPriData->mPrepareFinishResult = OK;
                        sem_post(&mPriData->mSemPrepareFinish);
                    }
                }
                else
                {
                    //* initialize player fail, notify a media error event.
                    mPriData->mStatus = AWPLAYER_STATUS_ERROR;
                    if(mPriData->mPrepareSync == 0)
                        sendEvent(MEDIA_ERROR, DEMUX_ERROR_UNKNOWN, 0);
                    else
                    {
                        mPriData->mPrepareFinishResult = UNKNOWN_ERROR;
                        sem_post(&mPriData->mSemPrepareFinish);
                    }
                }
            }

            break;
        }

        case AWPLAYER_MESSAGE_DEMUX_EOS:
        {
            logw("eos...");
            PlayerSetEos(mPriData->mPlayer);
            break;
        }

        case AWPLAYER_MESSAGE_DEMUX_IOERROR:
        {
            loge("io error...");
            //* should we report a MEDIA_INFO event of "MEDIA_INFO_NETWORK_ERROR" and
            //* try reconnect for sometimes before a MEDIA_ERROR_IO event reported ?
            sendEvent(MEDIA_ERROR, MEDIA_ERROR_IO, 0);
            if(mPriData->mYunOSInfoEndale)
            {
                mPriData->mYunOSErrorCode = 3004;
                sendEvent(MEDIA_INFO, MEDIA_INFO_UNKNOWN, YUNOS_HTTP_DOWNLOAD_ERROR_INFO);
            }
            break;
        }

        case AWPLAYER_MESSAGE_DEMUX_CACHE_REPORT:
        {
            int nTotalPercentage;
            int nBufferPercentage;
            int nLoadingPercentage;

            nTotalPercentage   = ((int*)param)[0];   //* read positon to total file size.
            nBufferPercentage  = ((int*)param)[1];   //* cache buffer fullness.
            nLoadingPercentage = ((int*)param)[2];   //* loading percentage to start play.

            if(!strcmp(mPriData->strApkName, "com.appgate.gorealra:remote"))
            {
                nTotalPercentage = 1;  //avoid apk force to stop.
            }
            sendEvent(MEDIA_BUFFERING_UPDATE, nTotalPercentage,
                    nBufferPercentage<<16 | nLoadingPercentage);
            break;
        }

        case AWPLAYER_MESSAGE_DEMUX_BUFFER_START:
        {
            sendEvent(MEDIA_INFO, MEDIA_INFO_BUFFERING_START);
            break;
        }

        case AWPLAYER_MESSAGE_DEMUX_BUFFER_END:
        {
            sendEvent(MEDIA_INFO, MEDIA_INFO_BUFFERING_END);
            break;
        }

        case AWPLAYER_MESSAGE_DEMUX_PAUSE_PLAYER:
        {
            /* be careful to check whether there is any player callback lock the mMutexStatus,
             * if so, the PlayerPause() call may fall into dead lock if the player
             * callback is requesting mMutexStatus.
             * currently we do not lock mMutexStatus in any player callback.
             */
            pthread_mutex_lock(&mPriData->mMutexStatus);

            if(mPriData->mStatus == AWPLAYER_STATUS_STARTED)
                PlayerPause(mPriData->mPlayer);

            pthread_mutex_unlock(&mPriData->mMutexStatus);
            break;
        }

        case AWPLAYER_MESSAGE_DEMUX_RESUME_PLAYER:
        {
            pthread_mutex_lock(&mPriData->mMutexStatus);

            if(mPriData->mStatus == AWPLAYER_STATUS_STARTED)
                PlayerStart(mPriData->mPlayer);

            pthread_mutex_unlock(&mPriData->mMutexStatus);
            break;
        }

#if(USE_NEW_BDMV_STREAM == 0)

        case AWPLAYER_MESSAGE_DEMUX_IOREQ_ACCESS:
        {
            char *filePath = (char *)((uintptr_t *)param)[0];
            int mode = ((uintptr_t *)param)[1];
            int *pRet = (int *)((uintptr_t *)param)[2];

            Parcel parcel;
            Parcel replyParcel;
            *pRet = -1;

            // write path string as a byte array
            parcel.writeInt32(strlen(filePath));
            parcel.write(filePath, strlen(filePath));
            parcel.writeInt32(mode);
            sendEvent(AWEXTEND_MEDIA_INFO, AWEXTEND_MEDIA_INFO_CHECK_ACCESS_RIGHRS,
                    0, &parcel, &replyParcel);
            replyParcel.setDataPosition(0);
            *pRet = replyParcel.readInt32();

            break;
        }

        case AWPLAYER_MESSAGE_DEMUX_IOREQ_OPEN:
        {
            char *filePath = (char *)((uintptr_t *)param)[0];
            int *pFd = (int *)((uintptr_t *)param)[1];
            int fd = -1;

            Parcel parcel;
            Parcel replyParcel;
            bool    bFdValid = false;

            *pFd = -1;

            // write path string as a byte array
            parcel.writeInt32(strlen(filePath));
            parcel.write(filePath, strlen(filePath));
            sendEvent(AWEXTEND_MEDIA_INFO, AWEXTEND_MEDIA_INFO_REQUEST_OPEN_FILE,
                    0, &parcel, &replyParcel);
            replyParcel.setDataPosition(0);
            bFdValid = replyParcel.readInt32();
            if (bFdValid == true)
            {
                fd = replyParcel.readFileDescriptor();
                if (fd < 0)
                {
                    loge("invalid fd '%d'", fd);
                    *pFd = -1;
                    break;
                }

                *pFd = dup(fd);
                if (*pFd < 0)
                {
                    loge("dup fd failure, errno(%d) '%d'", errno, fd);
                }
                close(fd);
            }

            break;
        }

        case AWPLAYER_MESSAGE_DEMUX_IOREQ_OPENDIR:
        {
            char *dirPath = (char *)((uintptr_t *)param)[0];
            int *pDirId = (int *)((uintptr_t *)param)[1];

            Parcel parcel;
            Parcel replyParcel;

            *pDirId = -1;

            // write path string as a byte array
            parcel.writeInt32(strlen(dirPath));
            parcel.write(dirPath, strlen(dirPath));
            sendEvent(AWEXTEND_MEDIA_INFO, AWEXTEND_MEDIA_INFO_REQUEST_OPEN_DIR,
                    0, &parcel, &replyParcel);
            replyParcel.setDataPosition(0);
            *pDirId = replyParcel.readInt32();
            break;
        }

        case AWPLAYER_MESSAGE_DEMUX_IOREQ_READDIR:
        {
            int dirId = ((uintptr_t *)param)[0];
            int *pRet = (int *)((uintptr_t *)param)[1];
            char *buf = (char *)((uintptr_t *)param)[2];
            int bufLen = ((uintptr_t *)param)[3];
            loge("** aw-read-dir: dirId = %d, buf = %p, bufLen = %d",
                 dirId,buf,bufLen);
            Parcel parcel;
            Parcel replyParcel;
            int fileNameLen = -1;
            int32_t replyRet = -1;

            *pRet = -1;

            // write path string as a byte array
            parcel.writeInt32(dirId);
            sendEvent(AWEXTEND_MEDIA_INFO, AWEXTEND_MEDIA_INFO_REQUEST_READ_DIR,
                    0, &parcel, &replyParcel);
            replyParcel.setDataPosition(0);
            replyRet = replyParcel.readInt32();

            if (0 == replyRet)
            {
                fileNameLen = replyParcel.readInt32();
                if (fileNameLen > 0 && fileNameLen < bufLen)
                {
                    const char* strdata = (const char*)replyParcel.readInplace(fileNameLen);
                    memcpy(buf, strdata, fileNameLen);
                    buf[fileNameLen] = 0;
                    *pRet = 0;
                }
            }
            break;
        }

        case AWPLAYER_MESSAGE_DEMUX_IOREQ_CLOSEDIR:
        {
            int dirId = ((uintptr_t *)param)[0];
            int *pRet = (int *)((uintptr_t *)param)[1];

            Parcel parcel;
            Parcel replyParcel;

            // write path string as a byte array
            parcel.writeInt32(dirId);
            sendEvent(AWEXTEND_MEDIA_INFO, AWEXTEND_MEDIA_INFO_REQUEST_CLOSE_DIR,
                    0, &parcel, &replyParcel);
            replyParcel.setDataPosition(0);
            *pRet = replyParcel.readInt32();

           break;
        }

#endif

        case AWPLAYER_MESSAGE_PLAYER_EOS:
        {
            mPriData->mStatus = AWPLAYER_STATUS_COMPLETE;
            if(mPriData->mLoop == 0)
            {
                logd("player notify eos.");
                mPriData->mSeekTime = 0; //* clear the seek flag.
                sendEvent(MEDIA_PLAYBACK_COMPLETE, 0, 0);
            }
            else
            {
                AwMessage msg;

                logv("player notify eos, loop is set, send start command.");
                mPriData->mSeekTime = 0;  //* seek to the file start and replay.
                //* send a start message.
                setMessage(&msg,
                           //* message id.
                           AWPLAYER_COMMAND_START,
                           //* params[0] = &mSemStart, internal message, do not post message.
                           0,
                           //* params[1] = &mStartReply, internal message, do not reply.
                           0);
                AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
            }
            break;
        }

        case AWPLAYER_MESSAGE_PLAYER_FIRST_PICTURE:
        {
            sendEvent(MEDIA_INFO, MEDIA_INFO_RENDERING_START, 0);
            DemuxCompNotifyFirstFrameShowed(mPriData->mDemux);
            break;
        }

        case AWPLAYER_MESSAGE_DEMUX_SEEK_FINISH:
        {
            int seekResult;
            int nSeekTimeMs;
            int nFinalSeekTimeMs;

            /* be careful to check whether there is any player callback lock the mMutexStatus,
             * if so, the PlayerPause() call may fall into dead lock if the player
             * callback is requesting mMutexStatus.
             * currently we do not lock mMutexStatus in any player callback.
             */
            pthread_mutex_lock(&mPriData->mMutexStatus);

            seekResult       = ((int*)param)[0];
            nSeekTimeMs      = ((int*)param)[1];
            nFinalSeekTimeMs = ((int*)param)[2];

            if (seekResult == 0)
            {
                PlayerReset(mPriData->mPlayer, ((int64_t)nFinalSeekTimeMs)*1000);

                if (nSeekTimeMs == mPriData->mSeekTime)
                {
                    mPriData->mSeeking = 0;
                    if (mPriData->mStatus == AWPLAYER_STATUS_STARTED)
                    {
                        PlayerStart(mPriData->mPlayer);
                        DemuxCompStart(mPriData->mDemux);
                    }
                }
                else
                {
                    logv("seek time not match, there may be another seek operation happening.");
                }
                pthread_mutex_unlock(&mPriData->mMutexStatus);
                sendEvent(MEDIA_SEEK_COMPLETE, 0, 0);
            }
            else
            {
                pthread_mutex_unlock(&mPriData->mMutexStatus);
                sendEvent(MEDIA_ERROR, MEDIA_ERROR_IO, 0);
            }
            break;
        }

        case AWPLAYER_MESSAGE_PLAYER_SUBTITLE_AVAILABLE:
        {
            Parcel parcel;
            unsigned int  nSubtitleId   = (unsigned int)((uintptr_t*)param)[0];
            SubtitleItem* pSubtitleItem = (SubtitleItem*)((uintptr_t*)param)[1];

            logd("subtitle available. id = %d, pSubtitleItem = %p",nSubtitleId,pSubtitleItem);
            if(pSubtitleItem == NULL)
            {
                logw("pSubtitleItem == NULL");
                break;
            }

            mPriData->mIsSubtitleInTextFormat = !!pSubtitleItem->bText;   //* 0 or 1.

            if(mPriData->mIsSubtitleDisable == 0)
            {
                if(mPriData->mIsSubtitleInTextFormat)
                {
                    SubtitleUtilsFillTextSubtitleToParcel(&parcel,
                                                          pSubtitleItem,
                                                          nSubtitleId,
                                                          mPriData->mDefaultTextFormat);
                }
                else
                {
                    //* clear the mSubtitleDisplayIds
                    memset(mPriData->mSubtitleDisplayIds,0xff,64*sizeof(unsigned int));
                    mPriData->mSubtitleDisplayIdsUpdateIndex = 0;
                    sendEvent(MEDIA_TIMED_TEXT);    //* clear bmp subtitle first
                    SubtitleUtilsFillBitmapSubtitleToParcel(&parcel, pSubtitleItem, nSubtitleId);
                }
                //*record subtitile id
                int index = mPriData->mSubtitleDisplayIdsUpdateIndex;
                mPriData->mSubtitleDisplayIds[index] = nSubtitleId;
                mPriData->mSubtitleDisplayIdsUpdateIndex++;
                if(mPriData->mSubtitleDisplayIdsUpdateIndex>=64)
                    mPriData->mSubtitleDisplayIdsUpdateIndex = 0;

                logd("notify available message.");
                sendEvent(MEDIA_TIMED_TEXT, 0, 0, &parcel);
            }

            break;
        }

        case AWPLAYER_MESSAGE_PLAYER_SUBTITLE_EXPIRED:
        {
            logd("subtitle expired.");
            Parcel       parcel;
            unsigned int nSubtitleId;
            int i;
            nSubtitleId = *(unsigned int*)param;

            if(mPriData->mIsSubtitleDisable == 0)
            {
                //* match the subtitle id which is displaying ,or we may clear null subtitle
                for(i=0;i<64;i++)
                {
                    if(nSubtitleId==mPriData->mSubtitleDisplayIds[i])
                        break;
                }

                if(i!=64)
                {
                    mPriData->mSubtitleDisplayIds[i] = 0xffffffff;
                    if(mPriData->mIsSubtitleInTextFormat == 1)
                    {
                        //* set subtitle id
                        parcel.writeInt32(KEY_GLOBAL_SETTING);

                        //* nofity app to hide this subtitle
                        parcel.writeInt32(KEY_STRUCT_AWEXTEND_HIDESUB);
                        parcel.writeInt32(1);
                        parcel.writeInt32(KEY_SUBTITLE_ID);
                        parcel.writeInt32(nSubtitleId);

                        logd("notify text expired message.");
                        sendEvent(MEDIA_TIMED_TEXT, 0, 0, &parcel);
                    }
                    else
                    {
                        //* clear the mSubtitleDisplayIds
                        memset(mPriData->mSubtitleDisplayIds,0xff,64*sizeof(unsigned int));
                        mPriData->mSubtitleDisplayIdsUpdateIndex = 0;
                        /* if the sub is bmp ,we just send "clear all"
                         * command, nSubtitleId is not sent.
                         */
                        logd("notify subtitle expired message.");
                        sendEvent(MEDIA_TIMED_TEXT);
                    }
                }
            }
            break;
        }

        case AWPLAYER_MESSAGE_PLAYER_VIDEO_SIZE:
        {
            int nWidth  = ((int*)param)[0];
            int nHeight = ((int*)param)[1];

            //* if had scale down, we should zoom the widht and height
            if(mPriData->mScaledownFlag == 1)
            {
                nWidth  = 2*nWidth;
                nHeight = 2*nHeight;
            }

            //* If the apk is baofengyinying.apk ,we should not callback video size
            //* to apk here, becauese we had callback when prepare.
            //* Baofengyinying.apk not allow callback two times.
            if(!strcmp(mPriData->strApkName, "com.storm.smart"))
            {
                mPriData->mCallbackVideoSizeInStartStatusFlag = 1;
            }

            if((nWidth != mPriData->mVideoSizeWidth || nHeight != mPriData->mVideoSizeHeight)
               || mPriData->mCallbackVideoSizeInStartStatusFlag == 0)
            {
                logi("video size : width %d, height %d, status %d, "
                     "mVideoSizeWidth %d, mVideoSizeHeight %d, flag %d",
                     nWidth, nHeight, mPriData->mStatus,
                     mPriData->mVideoSizeWidth,
                     mPriData->mVideoSizeHeight,
                     mPriData->mCallbackVideoSizeInStartStatusFlag);

                mPriData->mVideoSizeWidth  = nWidth;
                mPriData->mVideoSizeHeight = nHeight;
                sendEvent(MEDIA_SET_VIDEO_SIZE, mPriData->mVideoSizeWidth,
                        mPriData->mVideoSizeHeight);
                mPriData->mCallbackVideoSizeInStartStatusFlag = 1;
            }
            break;
        }

        case AWPLAYER_MESSAGE_PLAYER_AUDIORAWPLAY:
        {
            int64_t token = 0;
            static int raw_data_test = 0;
            int raw_flag = ((int*)param)[0];
            String8 raw1 = String8("raw_data_output=1");
            String8 raw0 = String8("raw_data_output=0");
            if((raw_flag && mPriData->mRawOccupyFlag) || (!raw_flag && !mPriData->mRawOccupyFlag))
            {
                break;
            }
            else
            {
            const sp<IAudioFlinger>& af = AudioSystem::get_audio_flinger();
                token = IPCThreadState::self()->clearCallingIdentity();
            if (af == 0)
            {
                loge("[star]............ PERMISSION_DENIED");
            }
            else
            {
                if (raw_flag)
                {
                    logv("[star]............ to set raw data output");
                    loge("occupy_pre ,mRawOccupyFlag:%d",mPriData->mRawOccupyFlag);
                    af->setParameters(0, raw1);
                    mPriData->mRawOccupyFlag = 1;
                    loge("occupy_post,mRawOccupyFlag:%d",mPriData->mRawOccupyFlag);
                }
                else
                {
                        logv("[star]............ to set not raw data output");
                        loge("release_pre,mRawOccupyFlag:%d",mPriData->mRawOccupyFlag);
                        af->setParameters(0, raw0);
                        mPriData->mRawOccupyFlag = 0;
                        loge("release_post,mRawOccupyFlag:%d",mPriData->mRawOccupyFlag);
                    }
                    IPCThreadState::self()->restoreCallingIdentity(token);
                }
            }
        }
            break;
        case AWPLAYER_MESSAGE_PLAYER_VIDEO_CROP:
            //* TODO
            break;

        case AWPLAYER_MESSAGE_PLAYER_VIDEO_UNSUPPORTED:

#if(CONFIG_PRODUCT == OPTION_PRODUCT_TVBOX)
            sendEvent(MEDIA_ERROR, MEDIA_ERROR_UNSUPPORTED, 0);
#endif
            if(mPriData->mYunOSInfoEndale)
            {
                mPriData->mYunOSErrorCode = 3001;
                sendEvent(MEDIA_INFO, MEDIA_INFO_UNKNOWN, YUNOS_HTTP_DOWNLOAD_ERROR_INFO);
            }
            break;

        case AWPLAYER_MESSAGE_PLAYER_AUDIO_UNSUPPORTED:
            if(mPriData->mMediaInfo->nVideoStreamNum == 0)
            {
                #if(CONFIG_PRODUCT == OPTION_PRODUCT_TVBOX)
                sendEvent(MEDIA_ERROR, MEDIA_ERROR_UNSUPPORTED, 0);
                #endif
            }
            //* TODO
            break;

        case AWPLAYER_MESSAGE_PLAYER_SUBTITLE_UNSUPPORTED:
            //* TODO
            break;

        case AWPLAYER_MESSAGE_DEMUX_VIDEO_STREAM_CHANGE:
            updateVideoInfo();
            break;
        case AWPLAYER_MESSAGE_DEMUX_AUDIO_STREAM_CHANGE:
            logw("it is not supported now.");
            break;

        case AWPLAYER_MESSAGE_PLAYER_SET_SECURE_BUFFER_COUNT:

            if(mPriData->mDemux != NULL)
               DemuxCompSetSecureBufferCount(mPriData->mDemux,param);
            else
               loge("the mDemux is null when set secure buffer count");

            logw("it is not supported now.");
            break;

        case AWPLAYER_MESSAGE_PLAYER_SET_SECURE_BUFFERS:

            if(mPriData->mDemux != NULL)
               DemuxCompSetSecureBuffers(mPriData->mDemux,param);
            else
               loge("the mDemux is null when set secure buffers");

            break;

        case AWPLAYER_MESSAGE_PLAYER_SET_AUDIO_INFO:
        {
            int* tmp = (int*)param;
            logd("**** audio info, %d, %d, %d, %d",
                 tmp[0],
                 tmp[1],
                 tmp[2],
                 tmp[3]);
            pthread_mutex_lock(&mPriData->mMutexMediaInfo);
            mPriData->mMediaInfo->pAudioStreamInfo[tmp[0]].nSampleRate = tmp[1];
            mPriData->mMediaInfo->pAudioStreamInfo[tmp[0]].nChannelNum = tmp[2];
            mPriData->mMediaInfo->pAudioStreamInfo[tmp[0]].nAvgBitrate = tmp[3];
            pthread_mutex_unlock(&mPriData->mMutexMediaInfo);
            break;
        }
        case AWPLAYER_MESSAGE_PLAYER_DOWNLOAD_START:
            break;
        case AWPLAYER_MESSAGE_PLAYER_DOWNLOAD_END:
        {
            if(mPriData->mYunOSInfoEndale)
            {
                sendEvent(MEDIA_INFO, MEDIA_INFO_UNKNOWN, YUNOS_TS_INFO);
            }
            break;
        }
        case AWPLAYER_MESSAGE_PLAYER_DOWNLOAD_ERROR:
        {
            if(mPriData->mYunOSInfoEndale)
            {
                mPriData->mYunOSErrorCode = *(int*)param;
                sendEvent(MEDIA_INFO, MEDIA_INFO_UNKNOWN, YUNOS_HTTP_DOWNLOAD_ERROR_INFO);
            }
            break;
        }
        default:
        {
            logw("message 0x%x not handled.", messageId);
            break;
        }
    }

    return OK;
}

#if(MUTE_DRM_WHEN_HDMI_FLAG)
void AwPlayer::HDMINotify(void* cookie, bool state)
{
    AwPlayer* p = (AwPlayer*)cookie;
    logd("*** call HDMINotify, p->mPlayer = %p, state = %d",p->mPriData->mPlayer,state);
    if(p->mPriData->mPlayer != NULL)
        PlayerSetAudioForceWriteToDeviceFlag(p->mPriData->mPlayer, state);
    else
        loge("the mPlayer is null when call HDMINotify()");

    return ;
}
#endif

static void* AwPlayerThread(void* arg)
{
    AwPlayer* me = (AwPlayer*)arg;
    me->mainThread();
    return NULL;
}


static int DemuxCallbackProcess(void* pUserData, int eMessageId, void* param)
{
    int       msg;
    AwPlayer* p;

    switch(eMessageId)
    {
        case DEMUX_NOTIFY_PREPARED:
            msg = AWPLAYER_MESSAGE_DEMUX_PREPARED;
            break;
        case DEMUX_NOTIFY_EOS:
            msg = AWPLAYER_MESSAGE_DEMUX_EOS;
            break;
        case DEMUX_NOTIFY_IOERROR:
            msg = AWPLAYER_MESSAGE_DEMUX_IOERROR;
            break;
        case DEMUX_NOTIFY_SEEK_FINISH:
            msg = AWPLAYER_MESSAGE_DEMUX_SEEK_FINISH;
            break;
        case DEMUX_NOTIFY_CACHE_STAT:
            msg = AWPLAYER_MESSAGE_DEMUX_CACHE_REPORT;
            break;
        case DEMUX_NOTIFY_BUFFER_START:
            msg = AWPLAYER_MESSAGE_DEMUX_BUFFER_START;
            break;
        case DEMUX_NOTIFY_BUFFER_END:
            msg = AWPLAYER_MESSAGE_DEMUX_BUFFER_END;
            break;
        case DEMUX_NOTIFY_PAUSE_PLAYER:
            msg = AWPLAYER_MESSAGE_DEMUX_PAUSE_PLAYER;
            break;
        case DEMUX_NOTIFY_RESUME_PLAYER:
            msg = AWPLAYER_MESSAGE_DEMUX_RESUME_PLAYER;
            break;

        case DEMUX_IOREQ_ACCESS:
            msg = AWPLAYER_MESSAGE_DEMUX_IOREQ_ACCESS;
            break;
        case DEMUX_IOREQ_OPEN:
            msg = AWPLAYER_MESSAGE_DEMUX_IOREQ_OPEN;
            break;
        case DEMUX_IOREQ_OPENDIR:
            msg = AWPLAYER_MESSAGE_DEMUX_IOREQ_OPENDIR;
            break;
        case DEMUX_IOREQ_READDIR:
            msg = AWPLAYER_MESSAGE_DEMUX_IOREQ_READDIR;
            break;
        case DEMUX_IOREQ_CLOSEDIR:
            msg = AWPLAYER_MESSAGE_DEMUX_IOREQ_CLOSEDIR;
            break;
        case DEMUX_VIDEO_STREAM_CHANGE:
            msg = AWPLAYER_MESSAGE_DEMUX_VIDEO_STREAM_CHANGE;
            break;
        case DEMUX_AUDIO_STREAM_CHANGE:
            msg = AWPLAYER_MESSAGE_DEMUX_AUDIO_STREAM_CHANGE;
            break;
        case STREAM_EVT_DOWNLOAD_START:
            msg = AWPLAYER_MESSAGE_PLAYER_DOWNLOAD_START;
            break;
        case STREAM_EVT_DOWNLOAD_END:
            msg = AWPLAYER_MESSAGE_PLAYER_DOWNLOAD_END;
            break;
        case STREAM_EVT_DOWNLOAD_DOWNLOAD_ERROR:
        {
            msg = AWPLAYER_MESSAGE_PLAYER_DOWNLOAD_ERROR;
            break;
        }

        default:
            logw("ignore demux callback message, eMessageId = 0x%x.", eMessageId);
            return -1;
    }

    p = (AwPlayer*)pUserData;
    p->callbackProcess(msg, param);

    return 0;
}


static int PlayerCallbackProcess(void* pUserData, int eMessageId, void* param)
{
    int       msg;
    AwPlayer* p;

    switch(eMessageId)
    {
        case PLAYER_NOTIFY_EOS:
            msg = AWPLAYER_MESSAGE_PLAYER_EOS;
            break;
        case PLAYER_NOTIFY_FIRST_PICTURE:
            msg = AWPLAYER_MESSAGE_PLAYER_FIRST_PICTURE;
            break;

        case PLAYER_NOTIFY_SUBTITLE_ITEM_AVAILABLE:
            msg = AWPLAYER_MESSAGE_PLAYER_SUBTITLE_AVAILABLE;
            break;

        case PLAYER_NOTIFY_SUBTITLE_ITEM_EXPIRED:
            msg = AWPLAYER_MESSAGE_PLAYER_SUBTITLE_EXPIRED;
            break;

        case PLAYER_NOTIFY_VIDEO_SIZE:
            msg = AWPLAYER_MESSAGE_PLAYER_VIDEO_SIZE;
            break;

        case PLAYER_NOTIFY_VIDEO_CROP:
            msg = AWPLAYER_MESSAGE_PLAYER_VIDEO_CROP;
            break;

        case PLAYER_NOTIFY_VIDEO_UNSUPPORTED:
            msg = AWPLAYER_MESSAGE_PLAYER_VIDEO_UNSUPPORTED;
            break;

        case PLAYER_NOTIFY_AUDIO_UNSUPPORTED:
            msg = AWPLAYER_MESSAGE_PLAYER_AUDIO_UNSUPPORTED;
            break;

        case PLAYER_NOTIFY_SUBTITLE_UNSUPPORTED:
            msg = AWPLAYER_MESSAGE_PLAYER_SUBTITLE_UNSUPPORTED;
            break;
        case PLAYER_NOTIFY_AUDIORAWPLAY:
            msg = AWPLAYER_MESSAGE_PLAYER_AUDIORAWPLAY;
            break;

        case PLAYER_NOTIFY_SET_SECURE_BUFFER_COUNT:
            msg = AWPLAYER_MESSAGE_PLAYER_SET_SECURE_BUFFER_COUNT;
            break;

        case PLAYER_NOTIFY_SET_SECURE_BUFFERS:
            msg = AWPLAYER_MESSAGE_PLAYER_SET_SECURE_BUFFERS;
            break;

        case PLAYER_NOTIFY_AUDIO_INFO:
            msg = AWPLAYER_MESSAGE_PLAYER_SET_AUDIO_INFO;
            break;
        case PLAYER_NOTIFY_VIDEO_RENDER_FRAME:
            return 0;

        default:
            logw("ignore player callback message, eMessageId = 0x%x.", eMessageId);
            return -1;
    }

    p = (AwPlayer*)pUserData;
    p->callbackProcess(msg, param);

    return 0;
}

static int GetCallingApkName(char* strApkName, int nMaxNameSize)
{
    int fd;

    sprintf(strApkName, "/proc/%d/cmdline", IPCThreadState::self()->getCallingPid());
    fd = ::open(strApkName, O_RDONLY);
    strApkName[0] = '\0';
    if (fd >= 0)
    {
        ::read(fd, strApkName, nMaxNameSize);
        ::close(fd);
        logd("Calling process is: %s", strApkName);
    }
    return 0;
}

