/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : cmccplayer.cpp
* Description : mediaplayer adapter for operator
* History :
*   Author  : AL3
*   Date    : 2015/05/05
*   Comment : first version
*
*/

#include "log.h"
#include "cmccplayer.h"
#include "demuxComponent.h"
#include "subtitleUtils.h"
#include "awStreamingSource.h"
//#include <AwPluginManager.h>
#include <string.h>

#include <media/Metadata.h>
#include <media/mediaplayer.h>
#include <binder/IPCThreadState.h>
#include <media/IAudioFlinger.h>
#include <fcntl.h>
#include <cutils/properties.h> // for property_get
#include <hardware/hwcomposer.h>

#include <version.h>

#include "player.h"             //* player library in "LIBRARY/PLAYER/"
#include "mediaInfo.h"
#include "demuxComponent.h"
#include "awMessageQueue.h"
#include "awLogRecorder.h"

#define FILE_AWPLAYER_CPP   //* use configuration in a awplayerConfig.h
#include "awplayerConfig.h"

//wasu livemode4 , apk set seekTo after start, we should call start after seek
#define CMCC_LIVEMODE_4_BUG (1)

// the cmcc apk bug, change channel when buffering icon is displayed,
// the buffering icon is not diappered, so we send a buffer end message in prepare/prepareAsync
#define BUFFERING_ICON_BUG (1)

// pause then start to display, the cmcc apk call seekTo, it will flush the buffering cache(livemod)
// or seek some frames in cache ( vod) , it will discontinue
#define PAUSE_THEN_SEEK_BUG (1)

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
static const int AWPLAYER_MESSAGE_DEMUX_HLS_DOWNLOAD_START        = 0x111;
static const int AWPLAYER_MESSAGE_DEMUX_HLS_DOWNLOAD_END        = 0x112;
static const int AWPLAYER_MESSAGE_DEMUX_HLS_DOWNLOAD_ERROR        = 0x113;
static const int AWPLAYER_MESSAGE_DEMUX_RESET_PLAYER            = 0x114;
static const int AWPLAYER_MESSAGE_DEMUX_LOG_RECORDER            = 0x115;
static const int AWPLAYER_MESSAGE_DEMUX_NOTIFY_TIMESHIFT_DURATION = 0x116;

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
static const int AWPLAYER_MESSAGE_PLAYER_VIDEO_RENDER_FRAME     = 0x20d;


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
static const int AWPLAYER_COMMAND_RESETURL      = 0x10b;
static const int AWPLAYER_COMMAND_SETSPEED      = 0x10c;
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
        pthread_mutex_t     mMutexMediaInfo;    //* for media info protection.
        //* for mStatus protection in start/stop/pause operation and complete/seek finish callback.
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
        sem_t               mSemPrepareFinish;//* for signal prepare finish, used in prepare().
        sem_t               mSemSetSpeed;

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
        int                 mSetSpeedReply;

        int                 mPrepareSync;   //* synchroized prarare() call, don't call back to user.
        int                 mSeeking;

        //* use to check whether seek callback is for current seek operation or previous.
        int                 mSeekTime;
        int                 mSeekSync;  //* internal seek, don't call back to user.
        int                 mLoop;
        int                 mKeepLastFrame;
        int                 mVideoSizeWidth;  //* use to record videoSize which had send to app
        int                 mVideoSizeHeight;

        enum CmccApplicationType mApplicationType;
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

        int                 mLivemode;
        int                 mPauseLivemode;
        bool                mbIsDiagnose;
        int64_t             mPauseTimeStamp;    //us
        int64_t             mShiftTimeStamp;    //us
        int                 mDisplayRatio;
        int64_t             mCurShiftTimeStamp; //us
        int64_t             mTimeShiftDuration; //ms

        AwLogRecorder*      mLogRecorder;
        char                mUri[1024];
        int                 mFirstStart;

        int                 mSeekTobug;

        // the cmcc player should change pause state when buffer start, to fix getposition bug
        int                 mDemuxNotifyPause;
        int64_t             mDemuxPauseTimeStamp;

        //*
        int64_t             mPlayTimeMs;
        int64_t             mBufferTimeMs;
        int                 mPreSeekTimeMs;

        int                 mSpeed;
        int                 mbFast;
        int                 mFastTime;

        int                 mScaledownFlag;
        int                 mUartLogLevel;
}PlayerPriData;


static void* AwPlayerThread(void* arg);
static int DemuxCallbackProcess(void* pUserData, int eMessageId, void* param);
static int PlayerCallbackProcess(void* pUserData, int eMessageId, void* param);
static int GetCallingApkName(char* strApkName, int nMaxNameSize);
static int ShiftTimeMode(int Shiftedms, char *buf);

void enableMediaBoost(MediaInfo* mi, int* logLevel);
void disableMediaBoost(int logLevel);

CmccPlayer::CmccPlayer()
{
    logd("cmccplayer construct.");
    //AwPluginInit();
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
    mPriData->mScaledownFlag =0;
    mPriData->mCurrentSelectTrackIndex = -1;
    mPriData->mLogRecorder = NULL;
    mPriData->mDemuxNotifyPause = 0;

#if    PAUSE_THEN_SEEK_BUG
    mPriData->mSeekTobug = 0;
#endif

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
    sem_init(&mPriData->mSemSetSpeed, 0, 0);

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
    mPriData->mApplicationType = CMCCNORMALPLAY;
    mPriData->mRawOccupyFlag = 0;

    mPriData->mLogRecorder = NULL;
    mPriData->mFirstStart = 1;

    strcpy(mPriData->mUri, "test");

    mPriData->mSpeed = 1;
    mPriData->mbFast = 0;
    mPriData->mFastTime = 0;

    GetCallingApkName(mPriData->strApkName, sizeof(mPriData->strApkName));
}


CmccPlayer::~CmccPlayer()
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
    sem_destroy(&mPriData->mSemSetSpeed);

    if(mPriData->mMediaInfo != NULL)
        clearMediaInfo();

    if(mPriData->mSourceUrl != NULL)
        free(mPriData->mSourceUrl);

    if(mPriData->mSourceFd != -1)
        ::close(mPriData->mSourceFd);

    if(mPriData->mIndexFileFdOfIndexSubtitle != -1)
        ::close(mPriData->mIndexFileFdOfIndexSubtitle);

    if(mPriData->mLogRecorder)
    {
        AwLogRecorderDestroy(mPriData->mLogRecorder);
        mPriData->mLogRecorder = NULL;
    }

    callbackProcess(AWPLAYER_MESSAGE_PLAYER_AUDIORAWPLAY, (void*)param_release);
    if (mPriData)
    {
        free(mPriData);
        mPriData = NULL;
    }
}


status_t CmccPlayer::initCheck()
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


status_t CmccPlayer::setUID(uid_t uid)
{
    logv("setUID(), uid = %d", uid);
    mPriData->mUID = uid;
    return OK;
}

#if (CONFIG_OS_VERSION >= OPTION_OS_VERSION_ANDROID_5_0)
status_t CmccPlayer::setDataSource(const sp<IMediaHTTPService> &httpService,
                                    const char* pUrl,
                                    const KeyedVector<String8,
                                    String8>* pHeaders)
#else
status_t CmccPlayer::setDataSource(const char* pUrl,
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

    if(strstr(pUrl, "livemode=1"))
    {
        mPriData->mLivemode = 1;
    }
    else if(strstr(pUrl, "livemode=2"))
    {
        mPriData->mLivemode = 2;
    }
    else if(strstr(pUrl, "livemode=4"))
    {
        mPriData->mLivemode = 4;
    }
    else
    {
        mPriData->mLivemode = 0;
    }

    //* for com.starcor.hunan, shishikandian & lunbo cannot seek to the right time,
    //* because url has no livemode info.
    if(strcmp(mPriData->strApkName, "com.starcor.hunan") == 0)
    {
        DemuxCompSetSpecificMode(mPriData->mDemux, 2);
        logd("xxxxxxxxxxxxxxxx mLivemode = 2");
    }

    if (strstr(pUrl, "diagnose=deep"))
    {
        ALOGD("In cmcc deep diagnose");
        mPriData->mbIsDiagnose = true;
    }
    else
    {
        mPriData->mbIsDiagnose = false;
    }
    logd("livemode = %d, mbIsDiagnose = %d", mPriData->mLivemode, mPriData->mbIsDiagnose);

    mPriData->mLogRecorder = AwLogRecorderCreate();

    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]setDataSource, url: %s",
                    LOG_TAG, __FUNCTION__, __LINE__, pUrl);
        logd("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }
    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]create a new player", LOG_TAG, __FUNCTION__, __LINE__);
        logd("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }

#if (CONFIG_OS_VERSION >= OPTION_OS_VERSION_ANDROID_5_0)

    mPriData->mHTTPService.clear();
    mPriData->mHTTPService = httpService;
    PlayerConfigDispErrorFrame(mPriData->mPlayer, 0);

    //* send a set data source message.
    setMessage(&msg,
               AWPLAYER_COMMAND_SET_SOURCE,             //* message id.
               (uintptr_t)&mPriData->mSemSetDataSource,    //* params[0] = &mSemSetDataSource.
               (uintptr_t)&mPriData->mSetDataSourceReply, //* params[1] = &mSetDataSourceReply.
               SOURCE_TYPE_URL,       //* params[2] = SOURCE_TYPE_URL.
               (uintptr_t)pUrl,       //* params[3] = pUrl.
               (uintptr_t)pHeaders,   //* params[4] = KeyedVector<String8, String8>* pHeaders;
               (uintptr_t)(mPriData->mHTTPService.get()));     //* params[5] = mHTTPService.get();

#else
    PlayerConfigDispErrorFrame(mPriData->mPlayer, 0);
    //* send a set data source message.
    setMessage(&msg,
               AWPLAYER_COMMAND_SET_SOURCE,             //* message id.
               (uintptr_t)&mPriData->mSemSetDataSource,        //* params[0] = &mSemSetDataSource.
               (uintptr_t)&mPriData->mSetDataSourceReply,      //* params[1] = &mSetDataSourceReply.
               SOURCE_TYPE_URL,            //* params[2] = SOURCE_TYPE_URL.
               (uintptr_t)pUrl,            //* params[3] = pUrl.
               (uintptr_t)pHeaders);       //* params[4] = KeyedVector<String8, String8>* pHeaders;
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
status_t CmccPlayer::setDataSource(int fd, int64_t offset, int64_t length)
{
    AwMessage msg;
    status_t ret;

    logv("setDataSource(fd), fd=%d", fd);

    PlayerConfigDispErrorFrame(mPriData->mPlayer, 0);
    //* send a set data source message.
    setMessage(&msg,
               AWPLAYER_COMMAND_SET_SOURCE,                 //* message id.
               (uintptr_t)&mPriData->mSemSetDataSource,     //* params[0] = &mSemSetDataSource.
               (uintptr_t)&mPriData->mSetDataSourceReply,   //* params[1] = &mSetDataSourceReply.
               SOURCE_TYPE_FD,                              //* params[2] = SOURCE_TYPE_FD.
               fd,                                          //* params[3] = fd.
               (uintptr_t)(offset>>32),              //* params[4] = high 32 bits of offset.
               (uintptr_t)(offset & 0xffffffff),     //* params[5] = low 32 bits of offset.
               (uintptr_t)(length>>32),              //* params[6] = high 32 bits of length.
               (uintptr_t)(length & 0xffffffff));    //* params[7] = low 32 bits of length.
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


status_t CmccPlayer::setDataSource(const sp<IStreamSource> &source)
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
    mPriData->mApplicationType = CMCCMIRACAST;
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
status_t CmccPlayer::setVideoSurfaceTexture(const sp<IGraphicBufferProducer>& bufferProducer)
{
    AwMessage msg;

    logv("setVideoSurfaceTexture, surface = %p", bufferProducer.get());

    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]setVideoSurfaceTexture", LOG_TAG, __FUNCTION__, __LINE__);
        logi("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }

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
status_t CmccPlayer::setVideoSurfaceTexture(const sp<ISurfaceTexture> &surfaceTexture)
{
    AwMessage msg;

    logd("setVideoSurfaceTexture, surface = %p", surfaceTexture.get());

    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]setVideoSurfaceTexture", LOG_TAG, __FUNCTION__, __LINE__);
        logi("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }

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


void CmccPlayer::setAudioSink(const sp<AudioSink> &audioSink)
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


status_t CmccPlayer::prepareAsync()
{
    AwMessage msg;

    logd("prepareAsync");

    struct timeval tv;
    int64_t startTimeMs = 0;
    int64_t endTimeMs;

    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]prepareAysnc start", LOG_TAG, __FUNCTION__, __LINE__);
        logi("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
        gettimeofday(&tv, NULL);
        startTimeMs = (tv.tv_sec * 1000000ll + tv.tv_usec)/1000;
    }

    //* send a prepare.
    setMessage(&msg,
               AWPLAYER_COMMAND_PREPARE,        //* message id.
               (uintptr_t)&mPriData->mSemPrepare,      //* params[0] = &mSemPrepare.
               (uintptr_t)&mPriData->mPrepareReply,    //* params[1] = &mPrepareReply.
               0);                              //* params[2] = mPrepareSync.
    AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
    SemTimedWait(&mPriData->mSemPrepare, -1);

    if(OK == mPriData->mPrepareReply && mPriData->mLogRecorder != NULL)
    {
        gettimeofday(&tv, NULL);
        endTimeMs = (tv.tv_sec * 1000000ll + tv.tv_usec)/1000;
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]Playback is ready, spend time: %lldms",
                LOG_TAG, __FUNCTION__, __LINE__, (long long int)(endTimeMs - startTimeMs));
        logi("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }

    return (status_t)mPriData->mPrepareReply;
}


status_t CmccPlayer::prepare()
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


status_t CmccPlayer::start()
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
                DemuxCompSetCachePolicy(mPriData->mDemux, (enum ECACHEPOLICY)cp->eCachePolicy,
                            cp->nStartPlaySize, cp->nStartPlayTimeMs, cp->nCacheBufferSize);
                break;
            }
            cp++;
        }
    }

    // in washu cmcc for livemode 1, we should call seekTo when paused longer than 30s
    // it is not correct in icntv
    mPriData->mLivemode = DemuxCompGetLiveMode(mPriData->mDemux);
    if(mPriData->mStatus == AWPLAYER_STATUS_PAUSED && mPriData->mLivemode == 1
        && !strcmp(mPriData->strApkName, "net.sunniwell.app.ott.chinamobile"))
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        int shiftTimeMs = (tv.tv_sec * 1000000ll + tv.tv_usec - mPriData->mPauseTimeStamp)/1000;
        if(shiftTimeMs > 30*1000 && !mPriData->mSeekTobug && (mPriData->mPauseLivemode!=2))
        {
            logd("pause time longer than 30s in livemode 1, seekTo(%d)", shiftTimeMs);
            seekTo(shiftTimeMs);
        }
    }

    // for livemode 4, cmcc apk invoke start before seekTo,
    // it will get a frame between start and seekTo,
#if CMCC_LIVEMODE_4_BUG
    if(mPriData->mLivemode == 4 && mPriData->mFirstStart &&
        !strcmp(mPriData->strApkName, "net.sunniwell.app.ott.chinamobile"))//tv.icntv.ott
    {
        return 0;
    }
#endif

#if (PAUSE_THEN_SEEK_BUG)
    if(mPriData->mSeekTobug && !strcmp(mPriData->strApkName, "net.sunniwell.app.ott.chinamobile"))
    {
        mPriData->mSeekTobug = 0;
        sendEvent(MEDIA_SEEK_COMPLETE, 0, 0);
    }
#endif

    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]start", LOG_TAG, __FUNCTION__, __LINE__);
        logi("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
        struct timeval tv;
        gettimeofday(&tv, NULL);
        mPriData->mPlayTimeMs = (tv.tv_sec * 1000000ll + tv.tv_usec)/1000;
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


status_t CmccPlayer::stop()
{
    AwMessage msg;

    logd("stop");

    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]stop", LOG_TAG, __FUNCTION__, __LINE__);
        logd("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }
    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]destory this player", LOG_TAG, __FUNCTION__, __LINE__);
        logi("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }

    //* send a stop message.
    setMessage(&msg,
               AWPLAYER_COMMAND_STOP,        //* message id.
               (uintptr_t)&mPriData->mSemStop,      //* params[0] = &mSemStop.
               (uintptr_t)&mPriData->mStopReply);   //* params[1] = &mStopReply.
    AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
    SemTimedWait(&mPriData->mSemStop, -1);

    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]Playback end", LOG_TAG, __FUNCTION__, __LINE__);
        logi("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }

    return (status_t)mPriData->mStopReply;
}


status_t CmccPlayer::pause()
{
    AwMessage msg;

    logd("pause");

    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]pause", LOG_TAG, __FUNCTION__, __LINE__);
        logi("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }

    mPriData->mLivemode = DemuxCompGetLiveMode(mPriData->mDemux);
    mPriData->mPauseLivemode = mPriData->mLivemode;
    if(mPriData->mLivemode == 1)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        mPriData->mPauseTimeStamp = tv.tv_sec * 1000000ll + tv.tv_usec;
        logd("livemode1, get current position inmPauseTimeStamp = %lld",
                mPriData->mPauseTimeStamp);
    }
    else if(mPriData->mLivemode == 2)
    {
        int msec;
        getCurrentPosition(&msec);
        logd("get current position in pause: %d", msec);

        struct timeval tv;
        gettimeofday(&tv, NULL);
        mPriData->mPauseTimeStamp = tv.tv_sec * 1000000ll + tv.tv_usec - (int64_t)msec*1000;
        logd("get current position in pause, nowUs = %lld, mPauseTimeStamp = %lld",
                tv.tv_sec * 1000000ll + tv.tv_usec, mPriData->mPauseTimeStamp);
    }

    //* send a pause message.
    setMessage(&msg,
               AWPLAYER_COMMAND_PAUSE,        //* message id.
               (uintptr_t)&mPriData->mSemPause,      //* params[0] = &mSemPause.
               (uintptr_t)&mPriData->mPauseReply);   //* params[1] = &mPauseReply.
    AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
    SemTimedWait(&mPriData->mSemPause, -1);
    return (status_t)mPriData->mPauseReply;
}

status_t CmccPlayer::seekTo(int nSeekTimeMs)
{
    AwMessage msg;

    logd("seekTo [%dms]", nSeekTimeMs);

    mPriData->mLivemode = DemuxCompGetLiveMode(mPriData->mDemux);

#if CMCC_LIVEMODE_4_BUG
    //for livemode4 , apk set seekTo after start, we should call start after seek
    if(mPriData->mLivemode == 4 && mPriData->mFirstStart &&
        !strcmp(mPriData->strApkName, "net.sunniwell.app.ott.chinamobile"))
    {
        status_t status;
        mPriData->mFirstStart = 0;
        status = seekTo(nSeekTimeMs);
        start();
        return status;
    }
#endif

#if (PAUSE_THEN_SEEK_BUG)
    if((mPriData->mStatus == AWPLAYER_STATUS_PAUSED
        /*|| mPriData->mStatus == AWPLAYER_STATUS_STARTED */) &&
        !strcmp(mPriData->strApkName, "net.sunniwell.app.ott.chinamobile"))
    {
        int pos;
        getCurrentPosition(&pos);
        int diff = pos - nSeekTimeMs;
        logd("diff = %d", diff);
        if(-1000 < diff && diff < 1000)
        {
            if(mPriData->mLivemode == 1 && mPriData->mStatus == AWPLAYER_STATUS_PAUSED)
            {
                // if livemod1, we should change it to livemode2
                struct timeval tv;
                gettimeofday(&tv, NULL);
                DemuxCompSetLiveMode(mPriData->mDemux, 2);
                mPriData->mShiftTimeStamp = tv.tv_sec * 1000000ll + tv.tv_usec - nSeekTimeMs*1000ll;
            }
            mPriData->mSeekTobug = 1;
            return start();
        }
    }
#endif

    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]seekTo, totime: %dms",
                LOG_TAG, __FUNCTION__, __LINE__, nSeekTimeMs);
        logi("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }

    if(mPriData->mLivemode == 1 || mPriData->mLivemode == 2)
    {
        logd("++++++ reset url");
        struct timeval tv;
        gettimeofday(&tv, NULL);
        mPriData->mShiftTimeStamp = tv.tv_sec * 1000000ll + tv.tv_usec;
        mPriData->mPreSeekTimeMs = nSeekTimeMs + 20000; //* for timeshift

        //* send a start message.
        setMessage(&msg,
                   AWPLAYER_COMMAND_RESETURL,    //* message id.
                   (uintptr_t)&mPriData->mSemSeek,      //* params[0] = &mSemSeek.
                   (uintptr_t)&mPriData->mSeekReply,    //* params[1] = &mSeekReply.
                   nSeekTimeMs,                  //* params[2] = mSeekTime.
                   0);                           //* params[3] = mSeekSync.
        AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
        SemTimedWait(&mPriData->mSemSeek, -1);
    }
    else
    {
        logd("seek");
        //* send a start message.
        setMessage(&msg,
                   AWPLAYER_COMMAND_SEEK,        //* message id.
                   (uintptr_t)&mPriData->mSemSeek,      //* params[0] = &mSemSeek.
                   (uintptr_t)&mPriData->mSeekReply,    //* params[1] = &mSeekReply.
                   nSeekTimeMs,                  //* params[2] = mSeekTime.
                   0);                           //* params[3] = mSeekSync.
        AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
        SemTimedWait(&mPriData->mSemSeek, -1);

    }

    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]seek complete", LOG_TAG, __FUNCTION__, __LINE__);
        logi("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }

    return (status_t)mPriData->mSeekReply;
}

int resumeMediaScanner(const char* strApkName)
{
    CEDARX_UNUSE(strApkName);
    return property_set("mediasw.stopscaner", "0");
}


status_t CmccPlayer::reset()
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

status_t CmccPlayer::setSpeed(int nSpeed)
{
    AwMessage msg;

    logw("reset...");
    mPriData->mSpeed = nSpeed;
    //PlayerSetSpeed(mPriData->mPlayer, nSpeed);

    if(nSpeed == 1)
    {
        mPriData->mbFast = 0;
        mPriData->mSpeed = 1;
        mPriData->mFastTime = 0;
        start();
        PlayerSetDiscardAudio(mPriData->mPlayer, 0);
        return 0;
    }

    //* send a start message.
    setMessage(&msg,
               AWPLAYER_COMMAND_SETSPEED,       //* message id.
               (uintptr_t)&mPriData->mSemSetSpeed,     //* params[0] = &mSemReset.
               (uintptr_t)&mPriData->mSetSpeedReply,
               nSpeed);                      //* params[2] = speed.
    AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
    SemTimedWait(&mPriData->mSemSetSpeed, -1);
    return (status_t)mPriData->mSetSpeedReply;
}
bool CmccPlayer::isPlaying()
{
    logv("isPlaying");
    if(mPriData->mStatus == AWPLAYER_STATUS_STARTED ||
        (mPriData->mStatus == AWPLAYER_STATUS_COMPLETE && mPriData->mLoop != 0))
        return true;
    else
        return false;
}


status_t CmccPlayer::getCurrentPosition(int* msec)
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

        mPriData->mLivemode = DemuxCompGetLiveMode(mPriData->mDemux);
        if(mPriData->mLivemode == 1)
        {
            if(mPriData->mStatus == AWPLAYER_STATUS_PAUSED)
            {
                struct timeval tv;
                gettimeofday(&tv, NULL);
                *msec = (tv.tv_sec * 1000000ll + tv.tv_usec - mPriData->mPauseTimeStamp)/1000;
            }
            else if(mPriData->mDemuxNotifyPause)
            {
                struct timeval tv;
                gettimeofday(&tv, NULL);
                *msec = (tv.tv_sec * 1000000ll + tv.tv_usec - mPriData->mDemuxPauseTimeStamp)/1000;
            }
            else
            {
                *msec = 0;
            }

            if(*msec < 0) *msec = 0;

            if(mPriData->mLogRecorder != NULL)
            {
                char cmccLog[4096] = "";
                sprintf(cmccLog, "[info][%s %s %d]current playtime: %dms",
                        LOG_TAG, __FUNCTION__, __LINE__, *msec);
                logi("AwLogRecorderRecord %s.", cmccLog);
                AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
            }

            return OK;
        }
        else if(mPriData->mLivemode == 2)
        {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            if(mPriData->mStatus == AWPLAYER_STATUS_PAUSED)
            {
                *msec = (tv.tv_sec * 1000000ll + tv.tv_usec - mPriData->mPauseTimeStamp)/1000;
                logd("livemode2, nowUs = %lld, mPauseTimeStamp = %lld",
                        tv.tv_sec * 1000000ll + tv.tv_usec, mPriData->mPauseTimeStamp);
            }
            else
            {
                //cmcc livemode2 only support ts
                nPositionUs = PlayerGetPositionCMCC(mPriData->mPlayer);
                logd("-- nPositionUs = %lld", nPositionUs);
                *msec = (tv.tv_sec * 1000000ll + tv.tv_usec -
                        mPriData->mShiftTimeStamp + nPositionUs)/1000;
            }
            if(*msec < 0)
            {
                logd("positon < 0 ,check it!!");
                *msec = 0;
            }
            logd("livemode = 2, getCurrentPosition %d", *msec);

            if(mPriData->mLogRecorder != NULL)
            {
                char cmccLog[4096] = "";
                sprintf(cmccLog, "[info][%s %s %d]current playtime: %dms",
                        LOG_TAG, __FUNCTION__, __LINE__, *msec);
                logi("AwLogRecorderRecord %s.", cmccLog);
                AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
            }

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
            else  //* generally, stream pts is started at 0 except ts stream.
                nPositionUs = PlayerGetPts(mPriData->mPlayer);
            *msec = (nPositionUs + 500)/1000;
            pthread_mutex_unlock(&mPriData->mMutexMediaInfo);

            if(mPriData->mLogRecorder != NULL)
            {
                char cmccLog[4096] = "";
                sprintf(cmccLog, "[info][%s %s %d]current playtime: %dms",
                            LOG_TAG, __FUNCTION__, __LINE__, *msec);
                logi("AwLogRecorderRecord %s.", cmccLog);
                AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
            }

            return OK;
        }
        else
        {
            loge("getCurrentPosition() fail, mMediaInfo==NULL.");
            *msec = 0;
            pthread_mutex_unlock(&mPriData->mMutexMediaInfo);

            if(mPriData->mLogRecorder != NULL)
            {
                char cmccLog[4096] = "";
                sprintf(cmccLog, "[info][%s %s %d]current playtime: %dms",
                            LOG_TAG, __FUNCTION__, __LINE__, *msec);
                logi("AwLogRecorderRecord %s.", cmccLog);
                AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
            }

            return OK;
        }
    }
    else
    {
        *msec = 0;

        if(mPriData->mLogRecorder != NULL)
        {
            char cmccLog[4096] = "";
            sprintf(cmccLog, "[info][%s %s %d]current playtime: %dms",
                    LOG_TAG, __FUNCTION__, __LINE__, *msec);
            logi("AwLogRecorderRecord %s.", cmccLog);
            AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
        }

        if(mPriData->mStatus == AWPLAYER_STATUS_ERROR)
            return INVALID_OPERATION;
        else
            return OK;
    }
}


status_t CmccPlayer::getDuration(int *msec)
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
            *msec = mPriData->mMediaInfo->nDurationMs;
        else
        {
            loge("getCurrentPosition() fail, mPriData->mMediaInfo==NULL.");
            *msec = 0;
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


status_t CmccPlayer::setLooping(int loop)
{
    logv("setLooping");

    if(mPriData->mStatus == AWPLAYER_STATUS_ERROR)
        return INVALID_OPERATION;

    mPriData->mLoop = loop;
    return OK;
}


player_type CmccPlayer::playerType()
{
    logv("playerType");
    return AW_PLAYER;
}


status_t CmccPlayer::invoke(const Parcel &request, Parcel *reply)
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
                    lang = " ";
                    reply->writeString16(String16(lang));
#if CONFIG_PRODUCT == OPTION_PRODUCT_TVBOX
                    reply->writeInt32(mPriData->mMediaInfo->pVideoStreamInfo[i].bIs3DStream);
                    //Please refer to the "enum EVIDEOCODECFORMAT" in "vdecoder.h"
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
                    //Please refer to the "enum EAUDIOCODECFORMAT" in "adecoder.h"
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
                    //Please refer to the "ESubtitleCodec" in "sdecoder.h"
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
                    fdSub = fd;           //* save the .sub file's descriptor to fdSub.
                    //* set the .idx file's descriptor to fd.
                    fd    = mPriData->mIndexFileFdOfIndexSubtitle;
                    mPriData->mIndexFileFdOfIndexSubtitle = -1;
                    mPriData->mIndexFileHasBeenSet = 0;//* clear the flag for adding more subtitle.
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
                        pNewStreamInfo[i].nReferenceVideoWidth  =
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
                pStreamInfo = (SubtitleStreamInfo*)malloc(sizeof(SubtitleStreamInfo)*
                        (mPriData->mMediaInfo->nSubtitleStreamNum + nStreamNum));
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

            nTrackCount = mPriData->mMediaInfo->nVideoStreamNum +
                            mPriData->mMediaInfo->nAudioStreamNum +
                            mPriData->mMediaInfo->nSubtitleStreamNum;
            if(nTrackCount == 0)
            {
                loge("can not switch audio or subtitle, there is no media stream.");
                return NO_INIT;
            }

            if(nStreamIndex >= mPriData->mMediaInfo->nVideoStreamNum &&
                nStreamIndex < (mPriData->mMediaInfo->nVideoStreamNum +
                    mPriData->mMediaInfo->nAudioStreamNum))
            {
                if(nMethodId == INVOKE_ID_SELECT_TRACK)
                {
                    //* switch audio.
                    nStreamIndex -= mPriData->mMediaInfo->nVideoStreamNum;
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
            else if(nStreamIndex >= (mPriData->mMediaInfo->nVideoStreamNum +
                        mPriData->mMediaInfo->nAudioStreamNum) &&
                        nStreamIndex < nTrackCount)
            {
                if(nMethodId == INVOKE_ID_SELECT_TRACK)
                {
                    mPriData->mCurrentSelectTrackIndex = nStreamIndex;
                    //* enable subtitle.
                    mPriData->mIsSubtitleDisable = 0;

                    //* switch subtitle.
                    nStreamIndex -= (mPriData->mMediaInfo->nVideoStreamNum +
                                    mPriData->mMediaInfo->nAudioStreamNum);
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
                loge("can not switch audio or subtitle to track %d, \
                    stream index exceed.(%d stream in total).",
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

            if(PlayerSet3DMode(mPriData->mPlayer, (enum EPICTURE3DMODE)ePicture3DMode,
                            (enum EDISPLAY3DMODE)eDisplay3DMode) != 0)
            {
                loge("call PlayerSet3DMode() return fail, ePicture3DMode = %d, \
                        eDisplay3DMode = %d.",
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

        default:
        {
            logv("unknown invode command %d", nMethodId);
            return UNKNOWN_ERROR;
        }
    }
}


status_t CmccPlayer::setParameter(int key, const Parcel &request)
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


status_t CmccPlayer::getParameter(int key, Parcel *reply)
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
#if (KEY_PARAMETER_GET == 1)
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


status_t CmccPlayer::getMetadata(const media::Metadata::Filter& ids, Parcel *records)
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

status_t CmccPlayer::updateVideoInfo()
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
    /*H3            H265 4K    -> do nothing (0)*/
    if(mi->pVideoStreamInfo->nWidth >= WIDTH_4K || mi->pVideoStreamInfo->nHeight >= HEIGHT_4K)
    {
        if (mi->pVideoStreamInfo->eCodecFormat == VIDEO_CODEC_FORMAT_H265)
        {
#if (H265_4K_CHECK_SCALE_DOWN == 1) //H3 need to support H265 4K P2P, do nothing
            logd("support H265 4K P2P.");
            ret = 0;
#endif
        }
        else{
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
int checkVideoSupported(const MediaInfo *mi)
{
    int ret = 0;    //default is supported

    /*A80/H8          H265 4K unsupported*/
    /*H3              H265 4K supported*/
    if (mi->pVideoStreamInfo->nWidth >= WIDTH_4K ||
        mi->pVideoStreamInfo->nHeight >= HEIGHT_4K)
    {
#if (SUPPORT_H265 == 0) //* ve support h265 on 1680, 4K is ok!
        if (mi->pVideoStreamInfo->eCodecFormat == VIDEO_CODEC_FORMAT_H265)
        {
            loge("Not support H265 4K video !!");
            ret = -1;
        }
#endif

    }
    //* for all chip, WMV1/WMV2/VP6 specs unsupported,
    //* but we still support to play, so return supported.
    else if (mi->pVideoStreamInfo->nWidth >= WIDTH_1080P ||
            mi->pVideoStreamInfo->nHeight >= HEIGHT_1080P)
    {
        if (mi->pVideoStreamInfo->eCodecFormat == VIDEO_CODEC_FORMAT_WMV1
                || mi->pVideoStreamInfo->eCodecFormat == VIDEO_CODEC_FORMAT_WMV2
                || mi->pVideoStreamInfo->eCodecFormat == VIDEO_CODEC_FORMAT_VP6)
        {
            loge("Not support WMV1/WMV2/VP6 1080P video !!");
#if (CONFIG_PRODUCT == OPTION_PRODUCT_PAD)
            ret = -1;
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

void enableMediaBoost(MediaInfo* mi, int* logLevel)
{
    char cmd[PROP_VALUE_MAX] = {0};
    int total = 0;
    struct ScMemOpsS *memops = NULL;
    char buf[10] = {0};
    int loglevel = 4;

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
        *logLevel = loglevel;
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
        logd("origin log level is %d, now set log level to 1", *logLevel);
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

status_t CmccPlayer::initializePlayer()
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
        if(checkVideoSupported(mi))
        {
            loge("this video is outof specs, unsupported.");
            return UNKNOWN_ERROR;
        }

        /*check if need scaledown*/
#if (CONFIG_PRODUCT == OPTION_PRODUCT_TVBOX)
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
#endif
       }

    //* initialize the player.
#if !AWPLAYER_CONFIG_DISABLE_VIDEO
    if(mi->pVideoStreamInfo != NULL)
    {
        #if(DROP_3D_SECOND_VIDEO_STREAM == 1)
            if(mi->pVideoStreamInfo->bIs3DStream == 1)
                mi->pVideoStreamInfo->bIs3DStream = 0;
        #endif
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
    }
#endif

    if(PlayerHasVideo(mPriData->mPlayer) == 0 && PlayerHasAudio(mPriData->mPlayer) == 0)
    {
        loge("neither video nor audio stream can be played.");
        pthread_mutex_unlock(&mPriData->mMutexMediaInfo);
        return UNKNOWN_ERROR;
    }

#if !AWPLAYER_CONFIG_DISABLE_SUBTITLE
#if (ENABLE_SUBTITLE_DISPLAY_IN_CEDARX == 1)
    PlayerConfigDispSubTitleInner(mPriData->mPlayer,
        !strcmp(mPriData->strApkName, "net.sunniwell.app.swplayer"));
#endif
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

        ret = PlayerSetSubtitleStreamInfo(mPriData->mPlayer,
                    mi->pSubtitleStreamInfo, mi->nSubtitleStreamNum, nDefaultSubtitleIndex);
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


void CmccPlayer::clearMediaInfo()
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


status_t CmccPlayer::mainThread()
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
                ret = DemuxCompSetUrlSource(mPriData->mDemux,
                                (void*)msg.params[5], mPriData->mSourceUrl, pHeaders);
#else
                ret = DemuxCompSetUrlSource(mPriData->mDemux,
                                mPriData->mSourceUrl, pHeaders);
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
                ret = DemuxCompSetFdSource(mPriData->mDemux,
                                mPriData->mSourceFd, mPriData->mSourceFdOffset,
                                mPriData->mSourceFdLength);
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

            logd("process message AWPLAYER_COMMAND_SET_SURFACE.");

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

            char prop_value[512];
            //* set scaling mode by default setting.
            property_get("epg.default_screen_ratio", prop_value, "1");
            int mode = NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW;
            logd("---- prop_value = %s", prop_value);
            if(atoi(prop_value) == 0)
            {
                logd("++++++++ IPTV_PLAYER_CONTENTMODE_LETTERBOX");
                mPriData->mDisplayRatio = 0;
            }
            else
            {
                logd( "+++++++ IPTV_PLAYER_CONTENTMODE_FULL");
                mPriData->mDisplayRatio = 1;
            }

            if(mPriData->mNativeWindow != NULL)
            {
                mPriData->mNativeWindow.get()->perform(mPriData->mNativeWindow.get(),
                                                    NATIVE_WINDOW_SETPARAMETER,
                                                    DISPLAY_CMD_SETSCREENRADIO,
                                                    mPriData->mDisplayRatio);
            }

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

            //* super class MediaPlayerInterface has mAudioSink.
            MediaPlayerInterface::setAudioSink(audioSink);

            if(pReplyValue != NULL)
                *pReplyValue = (int)OK;
            if(pReplySem != NULL)
                sem_post(pReplySem);
            continue;
        } //* end AWPLAYER_COMMAND_SET_AUDIOSINK.
        else if(msg.messageId == AWPLAYER_COMMAND_PREPARE)
        {
            logd("process message AWPLAYER_COMMAND_PREPARE.");
            if(mPriData->mStatus == AWPLAYER_STATUS_PREPARED)
            {
                //* for data source set by fd(file descriptor), the prepare() method
                //* is called in setDataSource(), so the player is already in PREPARED
                //* status, here we just notify a prepared message.

                //* when app call prepareAsync(), we callback video-size to app here,
                if(mPriData->mMediaInfo->pVideoStreamInfo != NULL)
                {
                    if(mPriData->mMediaInfo->pVideoStreamInfo->nWidth !=
                                mPriData->mVideoSizeWidth
                       && mPriData->mMediaInfo->pVideoStreamInfo->nHeight !=
                                mPriData->mVideoSizeHeight)
                    {
                        int nRotation;

                        nRotation = atoi((const char*)mPriData->mMediaInfo->cRotation);
                        if((nRotation%180)==0)//* when the rotation is 0 and 180
                        {
                            mPriData->mVideoSizeWidth  =
                                mPriData->mMediaInfo->pVideoStreamInfo->nWidth;
                            mPriData->mVideoSizeHeight =
                                mPriData->mMediaInfo->pVideoStreamInfo->nHeight;
                        }
                        else
                        {
                            //* when the rotation is 90 and 270,
                            //* we should exchange nHeight and nwidth
                            mPriData->mVideoSizeWidth  =
                                mPriData->mMediaInfo->pVideoStreamInfo->nHeight;
                            mPriData->mVideoSizeHeight =
                                mPriData->mMediaInfo->pVideoStreamInfo->nWidth;
                        }

                        logi("xxxxxxxxxx video size: width = %d, height = %d",
                                mPriData->mVideoSizeWidth, mPriData->mVideoSizeHeight);
                        sendEvent(MEDIA_SET_VIDEO_SIZE, mPriData->mVideoSizeWidth,
                                    mPriData->mVideoSizeHeight);
                    }
                }

                sendEvent(MEDIA_PREPARED, 0, 0);

#if BUFFERING_ICON_BUG
                if (!mPriData->mbIsDiagnose &&
                    !strcmp(mPriData->strApkName, "net.sunniwell.app.ott.chinamobile"))
                {
                    sendEvent(MEDIA_INFO, MEDIA_INFO_BUFFERING_END);
                }
#endif
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

            char prop_value[512];
            //* set scaling mode by default setting.
            property_get("epg.default_screen_ratio", prop_value, "1");
            int mode = NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW;
            logd("---- prop_value = %s", prop_value);
            if(atoi(prop_value) == 0)
            {
                logd("++++++++ IPTV_PLAYER_CONTENTMODE_LETTERBOX");
                mPriData->mDisplayRatio = 0;
            }
            else
            {
                logd( "+++++++ IPTV_PLAYER_CONTENTMODE_FULL");
                mPriData->mDisplayRatio = 1;
            }

            logd("-- mLogRecorder = %p, mDisplayRatio = %d",
                    mPriData->mLogRecorder, mPriData->mDisplayRatio);
            if(mPriData->mNativeWindow != NULL)
            {
                mPriData->mNativeWindow.get()->perform(mPriData->mNativeWindow.get(),
                                            NATIVE_WINDOW_SETPARAMETER,
                                            DISPLAY_CMD_SETSCREENRADIO,
                                            mPriData->mDisplayRatio);
            }

            //native_window_set_scaling_mode(mSurfaceTextureClient.get(), mode);
            //mode = NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW
            //mode = NATIVE_WINDOW_SCALING_MODE_SCALE_CROP
            // end
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
        else if(msg.messageId == AWPLAYER_COMMAND_SETSPEED)
        {
            logd("process message AWPLAYER_COMMAND_SETSPEED.");
            if(mPriData->mStatus != AWPLAYER_STATUS_PREPARED &&
               mPriData->mStatus != AWPLAYER_STATUS_STARTED )
            {
                logd("invalid start() call, player not in prepared, \
                        started, paused or complete status.");
                if(pReplyValue != NULL)
                    *pReplyValue = (int)INVALID_OPERATION;
                if(pReplySem != NULL)
                    sem_post(pReplySem);
                continue;
            }

            if(mPriData->mMediaInfo == NULL || mPriData->mMediaInfo->bSeekable == 0)
            {
                if(mPriData->mMediaInfo == NULL)
                {
                    loge("setspeed fail because mMediaInfo == NULL.");
                    if(pReplyValue != NULL)
                        *pReplyValue = (int)NO_INIT;
                    if(pReplySem != NULL)
                        sem_post(pReplySem);
                    continue;
                }
                else
                {
                    loge("media not seekable. cannot fast");
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

            int curTime;
            getCurrentPosition(&curTime);

            mPriData->mSeeking  = 1;
            mPriData->mbFast = 1;
            mPriData->mFastTime = curTime + mPriData->mSpeed*1000;
            mPriData->mSeekTime = mPriData->mFastTime;
            mPriData->mSeekSync = 0;
           pthread_mutex_unlock(&mPriData->mMutexStatus);

            //PlayerFastForeward(mPriData->mPlayer);
            PlayerSetDiscardAudio(mPriData->mPlayer, 1);

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
        }//* end AWPLAYER_COMMAND_SETSPEED.
        else if(msg.messageId == AWPLAYER_COMMAND_START)
        {
            logd("process message AWPLAYER_COMMAND_START.");
            if(mPriData->mStatus != AWPLAYER_STATUS_PREPARED &&
               mPriData->mStatus != AWPLAYER_STATUS_STARTED  &&
               mPriData->mStatus != AWPLAYER_STATUS_PAUSED   &&
               mPriData->mStatus != AWPLAYER_STATUS_COMPLETE)
            {
                logd("invalid start() call, player not in prepared, \
                    started, paused or complete status.");
                if(pReplyValue != NULL)
                    *pReplyValue = (int)INVALID_OPERATION;
                if(pReplySem != NULL)
                    sem_post(pReplySem);
                continue;
            }

            if(mPriData->mbFast == 1)
            {
                PlayerSetDiscardAudio(mPriData->mPlayer, 0);
                mPriData->mbFast = 0;
                mPriData->mSpeed = 1;
                mPriData->mFastTime = 0;
            }

            //* synchronize with the seek or complete callback and the status may be changed.
            pthread_mutex_lock(&mPriData->mMutexStatus);

            if(mPriData->mStatus == AWPLAYER_STATUS_STARTED)
            {
                if(PlayerGetStatus(mPriData->mPlayer) == PLAYER_STATUS_PAUSED
                        && mPriData->mSeeking == 0)
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
                               AWPLAYER_COMMAND_SEEK,   //* message id.
                               0, //* params[0] = &mSemSeek, internal message, do not post.
                               0, //* params[1] = &mSeekReply, internal message, do not set reply.
                               mPriData->mSeekTime,  //* params[2] = mSeekTime(ms).
                               1);    //* params[3] = mSeekSync.
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
                               AWPLAYER_COMMAND_STOP,   //* message id.
                               0,    //* params[0] = &mSemStop, internal message, do not post.
                               0);   //* params[1] = &mStopReply, internal message, do not reply.
                    AwMessageQueuePostMessage(mPriData->mMessageQueue, &newMsg);

                    //* post a prepare message.
                    setMessage(&newMsg,
                               AWPLAYER_COMMAND_PREPARE,    //* message id.
                               0,   //* params[0] = &mSemPrepare, internal message, do not post.
                               0,   //* params[1] = &mPrepareReply, internal message, do not reply.
                               1);  //* params[2] = mPrepareSync.
                    AwMessageQueuePostMessage(mPriData->mMessageQueue, &newMsg);

                    //* post a start message.
                    setMessage(&newMsg,
                               AWPLAYER_COMMAND_START,      //* message id.
                               0,    //* params[0] = &mSemStart, internal message, do not post.
                               0);   //* params[1] = &mStartReply, internal message, do not reply.
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

            if(mPriData->mApplicationType == CMCCMIRACAST)
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
                logd("invalid stop() call, player not in prepared, paused, \
                        started, stopped or complete status.");
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

            //* the prepare callback may happen at this moment.
            //* so the mStatus may be changed to PREPARED asynchronizely.
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
            PlayerClear(mPriData->mPlayer);               //* clear all media information in player.
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
             //* the prepare callback may happen at this moment.
            //* so the mStatus may be changed to PREPARED asynchronizely.
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

            //* stop and clear the demux.
            //* this will stop the seeking if demux is currently processing seeking message.
            //* it will clear the data source keep inside, this is important for the IStreamSource.
            DemuxCompStop(mPriData->mDemux);
            DemuxCompClear(mPriData->mDemux);

            //* stop and clear the player.
            PlayerStop(mPriData->mPlayer);
            PlayerClear(mPriData->mPlayer);   //* it will clear media info config to the player.

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
            logd("process message AWPLAYER_COMMAND_SEEK.");
            if(mPriData->mStatus != AWPLAYER_STATUS_PREPARED &&
               mPriData->mStatus != AWPLAYER_STATUS_STARTED  &&
               mPriData->mStatus != AWPLAYER_STATUS_PAUSED   &&
               mPriData->mStatus != AWPLAYER_STATUS_COMPLETE)
            {
                logd("invalid seekTo() call, player not in prepared, \
                            started, paused or complete status.");
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
        else if(msg.messageId == AWPLAYER_COMMAND_RESETURL)
        {
            logd("process message AWPLAYER_COMMAND_RESETURL.");
            if(mPriData->mStatus != AWPLAYER_STATUS_PREPARED &&
               mPriData->mStatus != AWPLAYER_STATUS_STARTED  &&
               mPriData->mStatus != AWPLAYER_STATUS_PAUSED   &&
               mPriData->mStatus != AWPLAYER_STATUS_COMPLETE)
            {
                logd("invalid seekTo() call, player not in prepared, started, \
                        paused or complete status.");
                if(pReplyValue != NULL)
                    *pReplyValue = (int)INVALID_OPERATION;
                if(pReplySem != NULL)
                    sem_post(pReplySem);
                continue;
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
            logd("seekTo %.2f secs", mPriData->mSeekTime / 1E3);
            pthread_mutex_unlock(&mPriData->mMutexStatus);

            if(PlayerGetStatus(mPriData->mPlayer) == PLAYER_STATUS_STOPPED)
            {
                //* if in prepared status, the player is in stopped status,
                //* this will make the player not record the nSeekTime at PlayerReset() operation
                //* called at seek finish callback.
                PlayerStart(mPriData->mPlayer);
            }
            PlayerPause(mPriData->mPlayer);
            DemuxCompSeekToResetUrl(mPriData->mDemux, mPriData->mSeekTime);

            if(pReplyValue != NULL)
                *pReplyValue = (int)OK;
            if(pReplySem != NULL)
                sem_post(pReplySem);
            continue;
        }
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

status_t CmccPlayer::setSubCharset(const char* strFormat)
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

status_t CmccPlayer::getSubCharset(char *charset)
{
    if(mPriData->mPlayer == NULL)
    {
        return -1;
    }

    strcpy(charset, mPriData->mDefaultTextFormat);

    return OK;
}

status_t CmccPlayer::setSubDelay(int nTimeMs)
{
    if(mPriData->mPlayer!=NULL)
        return PlayerSetSubtitleShowTimeAdjustment(mPriData->mPlayer, nTimeMs);
    else
        return -1;
}

int CmccPlayer::getSubDelay()
{
    if(mPriData->mPlayer!=NULL)
        return PlayerGetSubtitleShowTimeAdjustment(mPriData->mPlayer);
    else
        return -1;
}

status_t CmccPlayer::callbackProcess(int messageId, void* param)
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
                    {
                        if(mPriData->mLogRecorder != NULL)
                        {
                            char cmccLog[4096] = "";
                            sprintf(cmccLog, "[error][%s %s %d]Error happened, event: %d",
                                        LOG_TAG, __FUNCTION__, __LINE__, MEDIA_ERROR_IO);
                            logi("AwLogRecorderRecord %s.", cmccLog);
                            AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
                        }

                        sendEvent(MEDIA_ERROR, MEDIA_ERROR_IO, 0);
                    }
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
                            if(mPriData->mMediaInfo->pVideoStreamInfo->nWidth !=
                                        mPriData->mVideoSizeWidth
                               && mPriData->mMediaInfo->pVideoStreamInfo->nHeight !=
                                        mPriData->mVideoSizeHeight)
                            {
                                int nRotation;

                                nRotation = atoi((const char*)mPriData->mMediaInfo->cRotation);
                                if((nRotation%180)==0)//* when the rotation is 0 and 180
                                {
                                    mPriData->mVideoSizeWidth  =
                                            mPriData->mMediaInfo->pVideoStreamInfo->nWidth;
                                    mPriData->mVideoSizeHeight =
                                            mPriData->mMediaInfo->pVideoStreamInfo->nHeight;
                                }
                                else
                                {
                                    //* when the rotation is 90 and 270,
                                    //* we should exchange nHeight and nwidth
                                    mPriData->mVideoSizeWidth  =
                                            mPriData->mMediaInfo->pVideoStreamInfo->nHeight;
                                    mPriData->mVideoSizeHeight =
                                            mPriData->mMediaInfo->pVideoStreamInfo->nWidth;
                                }
                                logi("xxxxxxxxxx video size: width = %d, height = %d",
                                        mPriData->mVideoSizeWidth, mPriData->mVideoSizeHeight);
                                sendEvent(MEDIA_SET_VIDEO_SIZE, mPriData->mVideoSizeWidth,
                                            mPriData->mVideoSizeHeight);
                            }
                            else
                            {
                                //此时mVideoSizeWidth必是0
                                logi("xxxxxxxxxx video size: width = 0, height = 0");
                                sendEvent(MEDIA_SET_VIDEO_SIZE, 0, 0);
                            }
                        }
                        else
                        {
                            //此时mVideoSizeWidth必是0
                            logi("xxxxxxxxxx video size: width = 0, height = 0");
                            sendEvent(MEDIA_SET_VIDEO_SIZE, 0, 0);
                        }

#if BUFFERING_ICON_BUG
                        if (!mPriData->mbIsDiagnose &&
                            !strcmp(mPriData->strApkName, "net.sunniwell.app.ott.chinamobile"))
                        {
                            sendEvent(MEDIA_INFO, MEDIA_INFO_BUFFERING_END);
                        }
#endif
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
            if(mPriData->mLivemode == 2) //* timeshift
            {
                int seekTimeMs = 0;
                struct timeval tv;
                gettimeofday(&tv, NULL);
                mPriData->mCurShiftTimeStamp = tv.tv_sec * 1000000ll + tv.tv_usec;
                logd("mPriData->mCurShiftTimeStamp=%lld ms", mPriData->mCurShiftTimeStamp/1000);
                seekTimeMs = (mPriData->mCurShiftTimeStamp - mPriData->mShiftTimeStamp)/1000
                                - mPriData->mTimeShiftDuration + mPriData->mPreSeekTimeMs;
                logd("================ seekTimeMs=%d ms, mPriData->mCurShiftTimeStamp=%lld us,"
                       "mPriData->mShiftTimeStamp=%lld us,"
                       " mPriData->mTimeShiftDuration=%lld ms, mPreSeekTimeMs=%d ms",
                        seekTimeMs,
                        mPriData->mCurShiftTimeStamp, mPriData->mShiftTimeStamp,
                        mPriData->mTimeShiftDuration, mPriData->mPreSeekTimeMs);
                seekTo(seekTimeMs);
            }
            else
            {
                logw("eos...");
                PlayerSetEos(mPriData->mPlayer);
            }
            break;
        }

        case AWPLAYER_MESSAGE_DEMUX_IOERROR:
        {
            loge("io error...");
            //* should we report a MEDIA_INFO event of "MEDIA_INFO_NETWORK_ERROR" and
            //* try reconnect for sometimes before a MEDIA_ERROR_IO event reported ?

            if(mPriData->mLogRecorder != NULL)
            {
                char cmccLog[4096] = "";
                sprintf(cmccLog, "[error][%s %s %d]Error happened, event: %d",
                        LOG_TAG, __FUNCTION__, __LINE__, MEDIA_ERROR_IO);
                logi("AwLogRecorderRecord %s.", cmccLog);
                AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
            }

            sendEvent(MEDIA_ERROR, MEDIA_ERROR_IO, 0);
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
            sendEvent(MEDIA_BUFFERING_UPDATE, nTotalPercentage,
                        nBufferPercentage<<16 | nLoadingPercentage);
            break;
        }

        case AWPLAYER_MESSAGE_DEMUX_BUFFER_START:
        {
            logd("MEDIA_INFO_BUFFERING_START, mLogRecorder = %p,mDisplayRatio = %d",
                            mPriData->mLogRecorder ,mPriData->mDisplayRatio);

            if(mPriData->mLogRecorder != NULL)
            {
                char cmccLog[4096] = "";
                int *pNeedBufferSize = (int*)param;
                sprintf(cmccLog, "[info][%s %s %d]buffering start, need buffer data size: %.2fMB",
                                LOG_TAG, __FUNCTION__, __LINE__, *pNeedBufferSize/(1024*1024.0f));
                logi("AwLogRecorderRecord %s.", cmccLog);
                AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
                struct timeval tv;
                gettimeofday(&tv, NULL);
                mPriData->mBufferTimeMs = (tv.tv_sec * 1000000ll + tv.tv_usec)/1000;
            }

            // for live mode 1, getposition when network broken
            mPriData->mDemuxNotifyPause = 1;
            struct timeval tv;
            gettimeofday(&tv, NULL);
            mPriData->mDemuxPauseTimeStamp = tv.tv_sec * 1000000ll + tv.tv_usec;

            sendEvent(MEDIA_INFO, MEDIA_INFO_BUFFERING_START);
            break;
        }

        case AWPLAYER_MESSAGE_DEMUX_BUFFER_END:
        {
            logd("MEDIA_INFO_BUFFERING_END ...");

            if(mPriData->mLogRecorder != NULL)
            {
                struct timeval tv;
                gettimeofday(&tv, NULL);
                int64_t bufferEndTimeMs = (tv.tv_sec * 1000000ll + tv.tv_usec)/1000;
                char cmccLog[4096] = "";
                sprintf(cmccLog, "[info][%s %s %d]buffering end, lasting time: %lldms",
                            LOG_TAG, __FUNCTION__, __LINE__,
                            (long long int)(bufferEndTimeMs - mPriData->mBufferTimeMs));
                logi("AwLogRecorderRecord %s.", cmccLog);
                AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
            }

            mPriData->mDemuxNotifyPause = 0;
            mPriData->mDemuxPauseTimeStamp = 0;
            sendEvent(MEDIA_INFO, MEDIA_INFO_BUFFERING_END);
            break;
        }

        case AWPLAYER_MESSAGE_DEMUX_PAUSE_PLAYER:
        {
            logd("AWPLAYER_MESSAGE_DEMUX_PAUSE_PLAYER, mStatus = %d", mPriData->mStatus);
            //* be careful to check whether there is any player callback lock the mMutexStatus,
            //* if so, the PlayerPause() call may fall into dead lock if the player
            //* callback is requesting mMutexStatus.
            //* currently we do not lock mMutexStatus in any player callback.
            pthread_mutex_lock(&mPriData->mMutexStatus);
            if(mPriData->mStatus == AWPLAYER_STATUS_STARTED)
                PlayerPause(mPriData->mPlayer);

            pthread_mutex_unlock(&mPriData->mMutexStatus);
            break;
        }

        case AWPLAYER_MESSAGE_DEMUX_RESUME_PLAYER:
        {
            //* be careful to check whether there is any player callback lock the mMutexStatus,
            //* if so, the PlayerPause() call may fall into dead lock if the player
            //* callback is requesting mMutexStatus.
            //* currently we do not lock mMutexStatus in any player callback.
            pthread_mutex_lock(&mPriData->mMutexStatus);
            if(mPriData->mStatus == AWPLAYER_STATUS_STARTED /*&& bBuffering == false*/)
            {
                PlayerStart(mPriData->mPlayer);
            }
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
            sendEvent(AWEXTEND_MEDIA_INFO,
                    AWEXTEND_MEDIA_INFO_CHECK_ACCESS_RIGHRS,
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
            sendEvent(AWEXTEND_MEDIA_INFO,
                    AWEXTEND_MEDIA_INFO_REQUEST_OPEN_FILE,
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
            sendEvent(AWEXTEND_MEDIA_INFO,
                    AWEXTEND_MEDIA_INFO_REQUEST_OPEN_DIR,
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
            sendEvent(AWEXTEND_MEDIA_INFO,
                    AWEXTEND_MEDIA_INFO_REQUEST_CLOSE_DIR,
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
                           AWPLAYER_COMMAND_START,//* message id.
                           0,     //* params[0] = &mSemStart, internal message, do not post message.
                           0);    //* params[1] = &mStartReply, internal message, do not reply.
                AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
            }
            break;
        }

        case AWPLAYER_MESSAGE_PLAYER_FIRST_PICTURE:
        {
            sendEvent(MEDIA_INFO, MEDIA_INFO_RENDERING_START, 0);
            DemuxCompNotifyFirstFrameShowed(mPriData->mDemux);
            if(mPriData->mLogRecorder != NULL)
            {
                struct timeval tv;
                int64_t renderTimeMs;
                gettimeofday(&tv, NULL);
                renderTimeMs = (tv.tv_sec * 1000000ll + tv.tv_usec)/1000;
                char cmccLog[4096] = "";
                sprintf(cmccLog, "[info][%s %s %d]show the first video frame, spend time: %lldms",
                                LOG_TAG, __FUNCTION__, __LINE__,
                                (long long int)(renderTimeMs - mPriData->mPlayTimeMs));
                logi("AwLogRecorderRecord %s.", cmccLog);
                AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
            }
            break;
        }
        case AWPLAYER_MESSAGE_DEMUX_RESET_PLAYER:
        {
            logd("AWPLAYER_MESSAGE_DEMUX_RESET_PLAYER");
            mPriData->mLivemode = DemuxCompGetLiveMode(mPriData->mDemux);
            if(mPriData->mLivemode == 1)
            {
                mPriData->mSeekTime = 0;
            }
            pthread_mutex_lock(&mPriData->mMutexStatus);
            PlayerReset(mPriData->mPlayer, ((int64_t)mPriData->mSeekTime)*1000);
            pthread_mutex_unlock(&mPriData->mMutexStatus);
            break;
        }
        case AWPLAYER_MESSAGE_DEMUX_SEEK_FINISH:
        {
            logd("AWPLAYER_MESSAGE_DEMUX_SEEK_FINISH");
            int seekResult;
            int nSeekTimeMs;
            int nFinalSeekTimeMs;

            //* be careful to check whether there is any player callback lock the mMutexStatus,
            //* if so, the PlayerPause() call may fall into dead lock if the player
            //* callback is requesting mMutexStatus.
            //* currently we do not lock mMutexStatus in any player callback.
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
            else if(seekResult == DEMUX_ERROR_USER_CANCEL)
            {
                // do nothing , do not start player
                pthread_mutex_unlock(&mPriData->mMutexStatus);
                sendEvent(MEDIA_SEEK_COMPLETE, 0, 0);
            }
            else
            {
                pthread_mutex_unlock(&mPriData->mMutexStatus);

                if(mPriData->mLogRecorder != NULL)
                {
                    char cmccLog[4096] = "";
                    sprintf(cmccLog, "[error][%s %s %d]Error happened, event: %d",
                            LOG_TAG, __FUNCTION__, __LINE__, MEDIA_ERROR_IO);
                    logi("AwLogRecorderRecord %s.", cmccLog);
                    AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
                }
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
                mPriData->mSubtitleDisplayIds[mPriData->mSubtitleDisplayIdsUpdateIndex] =
                                    nSubtitleId;
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
                        //* if the sub is bmp ,we just send "clear all" command,
                        //* nSubtitleId is not sent.
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

            //if(nWidth != mVideoSizeWidth || nHeight != mVideoSizeHeight)
            {
                logi("xxxxxxxxxx video size : width = %d, height = %d", nWidth, nHeight);

                mPriData->mVideoSizeWidth  = nWidth;
                mPriData->mVideoSizeHeight = nHeight;
                sendEvent(MEDIA_SET_VIDEO_SIZE, mPriData->mVideoSizeWidth,
                            mPriData->mVideoSizeHeight);
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
            break;

        case AWPLAYER_MESSAGE_PLAYER_AUDIO_UNSUPPORTED:
            if(mPriData->mMediaInfo->nVideoStreamNum == 0)
            {
                #if(CONFIG_PRODUCT == OPTION_PRODUCT_TVBOX)

                if(mPriData->mLogRecorder != NULL)
                {
                    char cmccLog[4096] = "";
                    sprintf(cmccLog, "[error][%s %s %d]Error happened, event: %d",
                            LOG_TAG, __FUNCTION__, __LINE__, MEDIA_ERROR_UNSUPPORTED);
                    logi("AwLogRecorderRecord %s.", cmccLog);
                    AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
                }

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

        case AWPLAYER_MESSAGE_PLAYER_VIDEO_RENDER_FRAME:
            if(mPriData->mbFast)
            {
                logd("==== video key frame in fast mode, mFastTime: %d, mSpeed: %d",
                            mPriData->mFastTime, mPriData->mSpeed);
                if(mPriData->mSpeed == 0)
                {
                    break;
                }

                int sleepTime = (mPriData->mSpeed > 0) ? 2000000/mPriData->mSpeed :
                            (-2000000/mPriData->mSpeed);
                //usleep(sleepTime);

                mPriData->mFastTime += mPriData->mSpeed*1000;
                if(mPriData->mFastTime > 0 && mPriData->mbFast)
                {
                    AwMessage msg;

                    //* send a seek message.
                    setMessage(&msg,
                               AWPLAYER_COMMAND_SEEK,        //* message id.
                               (uintptr_t)&mPriData->mSemSeek,      //* params[0] = &mSemSeek.
                               (uintptr_t)&mPriData->mSeekReply,    //* params[1] = &mSeekReply.
                               mPriData->mFastTime,                  //* params[2] = mSeekTime.
                               0);                           //* params[3] = mSeekSync.
                    AwMessageQueuePostMessage(mPriData->mMessageQueue, &msg);
                }
                else if(mPriData->mFastTime <= 0)
                {
                    PlayerSetDiscardAudio(mPriData->mPlayer, 0);
                    mPriData->mbFast = 0;
                }
            }
            break;

        case AWPLAYER_MESSAGE_DEMUX_HLS_DOWNLOAD_START:
        {
            logd("--- hls message, AWPLAYER_MESSAGE_DEMUX_HLS_DOWNLOAD_START");
            DownloadObject* obj = (DownloadObject*)param;
            if(mPriData->mLogRecorder != NULL)
            {
                char cmccLog[4096] = "";
                sprintf(cmccLog, "[info][%s %s %d]ts segment download is start, \
                            number: %d, filesize: %.2fMB, duration: %lldms",
                            LOG_TAG, __FUNCTION__, __LINE__, obj->seqNum,
                            (float)obj->seqSize/(1024*1024.0f), obj->seqDuration/1000);
                logi("AwLogRecorderRecord %s.", cmccLog);
                AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
            }
            sendEvent(MEDIA_INFO, MEDIA_INFO_DOWNLOAD_START, obj->seqNum);
            break;
        }
        case AWPLAYER_MESSAGE_DEMUX_HLS_DOWNLOAD_END:
        {
            DownloadObject* obj = (DownloadObject*)param;
            if(mPriData->mLogRecorder != NULL)
            {
                char cmccLog[4096] = "";
                sprintf(cmccLog, "[info][%s %s %d]ts segment download is complete, \
                                recvsize: %.2fMB, spend time: %lldms, rate: %lldbps",
                                LOG_TAG, __FUNCTION__, __LINE__,
                                (float)obj->seqSize/(1024*1024.0f), obj->spendTime, obj->rate);
                logi("AwLogRecorderRecord %s.", cmccLog);
                AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
            }
            sendEvent(MEDIA_INFO, MEDIA_INFO_DOWNLOAD_END, (int)obj->spendTime);
            break;
        }
        case AWPLAYER_MESSAGE_DEMUX_HLS_DOWNLOAD_ERROR:
        {
            DownloadObject* obj = (DownloadObject*)param;
            logd("---- hls message, need mediaplayer.cpp support");

            if(mPriData->mLogRecorder != NULL)
            {
                char cmccLog[4096] = "";
                sprintf(cmccLog, "[error][%s %s %d]Error happened, event: %d", LOG_TAG,
                        __FUNCTION__, __LINE__, MEDIA_INFO_DOWNLOAD_ERROR);
                logi("AwLogRecorderRecord %s.", cmccLog);
                AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
            }

            sendEvent(MEDIA_INFO, MEDIA_INFO_DOWNLOAD_ERROR, obj->statusCode);

            break;
        }
        case AWPLAYER_MESSAGE_DEMUX_LOG_RECORDER:
        {
            if(mPriData->mLogRecorder != NULL)
            {
                logi("AwLogRecorderRecord %s.", (char*)param);
                AwLogRecorderRecord(mPriData->mLogRecorder, (char*)param);
            }
            break;
        }
        case AWPLAYER_MESSAGE_DEMUX_NOTIFY_TIMESHIFT_DURATION:
        {
            if(mPriData->mLivemode == 2)
            {
                mPriData->mTimeShiftDuration = *(cdx_int64*)param;
                logd("xxxxxxxxx mPriData->mTimeShiftDuration=%lld", mPriData->mTimeShiftDuration);
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


static void* AwPlayerThread(void* arg)
{
    CmccPlayer* me = (CmccPlayer*)arg;
    me->mainThread();
    return NULL;
}


static int DemuxCallbackProcess(void* pUserData, int eMessageId, void* param)
{
    int       msg;
    CmccPlayer* p;

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
        case DEMUX_NOTIFY_RESET_PLAYER:
            msg = AWPLAYER_MESSAGE_DEMUX_RESET_PLAYER;
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

        case DEMUX_NOTIFY_HLS_DOWNLOAD_START:
            msg = AWPLAYER_MESSAGE_DEMUX_HLS_DOWNLOAD_START;
            break;
        case DEMUX_NOTIFY_HLS_DOWNLOAD_END:
            msg = AWPLAYER_MESSAGE_DEMUX_HLS_DOWNLOAD_END;
            break;
        case DEMUX_NOTIFY_HLS_DOWNLOAD_ERROR:
            msg = AWPLAYER_MESSAGE_DEMUX_HLS_DOWNLOAD_ERROR;
            break;
        case DEMUX_NOTIFY_LOG_RECORDER:
            msg = AWPLAYER_MESSAGE_DEMUX_LOG_RECORDER;
            break;
        case DEMUX_NOTIFY_TIMESHIFT_DURATION:
            msg = AWPLAYER_MESSAGE_DEMUX_NOTIFY_TIMESHIFT_DURATION;
            break;
        default:
            logw("ignore demux callback message, eMessageId = 0x%x.", eMessageId);
            return -1;
    }

    p = (CmccPlayer*)pUserData;
    p->callbackProcess(msg, param);

    return 0;
}


static int PlayerCallbackProcess(void* pUserData, int eMessageId, void* param)
{
    int       msg;
    CmccPlayer* p;

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

        case PLAYER_NOTIFY_VIDEO_RENDER_FRAME:
            msg = AWPLAYER_MESSAGE_PLAYER_VIDEO_RENDER_FRAME;
            break;
        default:
            logw("ignore player callback message, eMessageId = 0x%x.", eMessageId);
            return -1;
    }

    p = (CmccPlayer*)pUserData;
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

