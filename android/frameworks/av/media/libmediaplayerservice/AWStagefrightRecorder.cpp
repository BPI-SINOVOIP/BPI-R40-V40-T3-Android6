//fengyun add for recorder start 20160601
/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_NDEBUG 0
#define LOG_TAG "AWStagefrightRecorder"

#include <utils/Log.h>

#include "AWStagefrightRecorder.h"

#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>

#include <media/IMediaPlayerService.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/ALooper.h>

#include <media/stagefright/AudioSource.h>
#include <media/stagefright/AMRWriter.h>
#include <media/stagefright/AACWriter.h>
#include <media/stagefright/CameraSource.h>
#include <media/stagefright/CameraSourceTimeLapse.h>
#include <media/stagefright/MPEG2TSWriter.h>
#include <media/stagefright/MPEG4Writer.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/OMXClient.h>
#include <media/stagefright/OMXCodec.h>
#include <media/stagefright/SurfaceMediaSource.h>
#include <media/MediaProfiles.h>
#include <media/stagefright/MediaCodec.h>
#include <media/stagefright/MediaMuxer.h>
#include <gui/Surface.h>
#include <media/ICrypto.h>
#include <camera/ICamera.h>
#include <camera/CameraParameters.h>
#include <gui/Surface.h>

#include <utils/Errors.h>
#include <sys/types.h>
#include <ctype.h>
#include <unistd.h>

#include <system/audio.h>

#include "ARTPWriter.h"
#include <media/stagefright/Utils.h> 
#include "vencoder.h"

#define DROP_FRAME 135

/*  Four-character-code (FOURCC) */

#define v4l2_fourcc(a, b, c, d)\	
	((__u32)(a) | ((__u32)(b) << 8) | ((__u32)(c) << 16) | ((__u32)(d) << 24))

#define V4L2_PIX_FMT_H264     v4l2_fourcc('H', '2', '6', '4') /* H264 with start codes */

namespace android {

static const uint8_t kNalUnitTypeSeqParamSet = 0x07;
static const uint8_t kNalUnitTypePicParamSet = 0x08;


#define ENCODE_TIMEOUT 250000
// To collect the encoder usage for the battery app
static void addBatteryData(uint32_t params) {
    sp<IBinder> binder =
        defaultServiceManager()->getService(String16("media.player"));
    sp<IMediaPlayerService> service = interface_cast<IMediaPlayerService>(binder);
    CHECK(service.get() != NULL);

    service->addBatteryData(params);
}


AWStagefrightRecorder::AWStagefrightRecorder(const String16 &opPackageName)
	: MediaRecorderBase(opPackageName),
      mVideoCodec(NULL),
      mAudioCodec(NULL),
      mCameraSource(NULL),
      mAudioSource(NULL),
      mMediaMuxer(NULL),
      mMediaMuxerNext(NULL),
      mAudioTrackMeta(NULL),
      mOutputFd(-1),
	  mOutputPath(NULL),
	  mCaptureTimeLapse(false),
      mAudioSourceType(AUDIO_SOURCE_CNT),
      mVideoSource(VIDEO_SOURCE_LIST_END),
	  mSurfaceMediaSource(NULL),
	  mCodecSpecificData(NULL),
	  mCodecSpecificDataSize(0),
      mStarted(false) {
    ALOGV("Constructor");
    reset();
}

AWStagefrightRecorder::~AWStagefrightRecorder() {
    ALOGV("Destructor");
    stop();
	if(mOutputPath != NULL) {
        free(mOutputPath);
        mOutputPath = NULL;
    }
    if (mLooper != NULL) {
        mLooper->stop();
    }
}

status_t AWStagefrightRecorder::init() {
    ALOGV("init");
    mLooper = new ALooper;
    mLooper->setName("recorder_looper");
    mLooper->start();

    return OK;
}

// The client side of mediaserver asks it to creat a SurfaceMediaSource
// and return a interface reference. The client side will use that
// while encoding GL Frames
sp<IGraphicBufferProducer> AWStagefrightRecorder::querySurfaceMediaSource() const {
    ALOGV("Get SurfaceMediaSource");
    return NULL;
}

status_t AWStagefrightRecorder::setAudioSource(audio_source_t as) {
    ALOGV("setAudioSource: %d", as);
    if (as < AUDIO_SOURCE_DEFAULT ||
        as >= AUDIO_SOURCE_CNT) {
        ALOGE("Invalid audio source: %d", as);
        return BAD_VALUE;
    }

    if (as == AUDIO_SOURCE_DEFAULT) {
        mAudioSourceType = AUDIO_SOURCE_MIC;
    } else {
        mAudioSourceType = as;
    }

    return OK;
}

status_t AWStagefrightRecorder::setVideoSource(video_source vs) {
    ALOGV("setVideoSource: %d", vs);
    if (vs < VIDEO_SOURCE_DEFAULT ||
        vs >= VIDEO_SOURCE_LIST_END) {
        ALOGE("Invalid video source: %d", vs);
        return BAD_VALUE;
    }

    if (vs == VIDEO_SOURCE_DEFAULT) {
        mVideoSource = VIDEO_SOURCE_CAMERA;
    } else {
        mVideoSource = vs;
    }

    return OK;
}

status_t AWStagefrightRecorder::setOutputFormat(output_format of) {
    ALOGV("setOutputFormat: %d", of);
    if (of < OUTPUT_FORMAT_DEFAULT ||
        of >= OUTPUT_FORMAT_LIST_END) {
        ALOGE("Invalid output format: %d", of);
        return BAD_VALUE;
    }

    if (of == OUTPUT_FORMAT_DEFAULT) {
        mOutputFormat = OUTPUT_FORMAT_THREE_GPP;
    } else {
        mOutputFormat = of;
    }

    return OK;
}

status_t AWStagefrightRecorder::setAudioEncoder(audio_encoder ae) {
    ALOGV("setAudioEncoder: %d", ae);
    if (ae < AUDIO_ENCODER_DEFAULT ||
        ae >= AUDIO_ENCODER_LIST_END) {
        ALOGE("Invalid audio encoder: %d", ae);
        return BAD_VALUE;
    }

    if (ae == AUDIO_ENCODER_DEFAULT) {
        mAudioEncoder = AUDIO_ENCODER_AMR_NB;
    } else {
        mAudioEncoder = ae;
    }

    return OK;
}

status_t AWStagefrightRecorder::setVideoEncoder(video_encoder ve) {
    ALOGV("setVideoEncoder: %d", ve);
    if (ve < VIDEO_ENCODER_DEFAULT ||
        ve >= VIDEO_ENCODER_LIST_END) {
        ALOGE("Invalid video encoder: %d", ve);
        return BAD_VALUE;
    }

    if (ve == VIDEO_ENCODER_DEFAULT) {
        //mVideoEncoder = VIDEO_ENCODER_H263;
        mVideoEncoder = VIDEO_ENCODER_H264;
    } else {
        mVideoEncoder = ve;
    }

    return OK;
}

status_t AWStagefrightRecorder::setVideoSize(int width, int height) {
    ALOGV("setVideoSize: %dx%d", width, height);
    if (width <= 0 || height <= 0) {
        ALOGE("Invalid video size: %dx%d", width, height);
        return BAD_VALUE;
    }

    // Additional check on the dimension will be performed later
    mVideoWidth = width;
    mVideoHeight = height;

    return OK;
}

status_t AWStagefrightRecorder::setVideoFrameRate(int frames_per_second) {
    ALOGV("setVideoFrameRate: %d", frames_per_second);
    if ((frames_per_second <= 0 && frames_per_second != -1) ||
        frames_per_second > 120) {
        ALOGE("Invalid video frame rate: %d", frames_per_second);
        return BAD_VALUE;
    }

    // Additional check on the frame rate will be performed later
    mFrameRate = frames_per_second;

    return OK;
}

status_t AWStagefrightRecorder::setCamera(const sp<ICamera> &camera,
                                        const sp<ICameraRecordingProxy> &proxy) {
    ALOGV("setCamera");
    if (camera == 0) {
        ALOGE("camera is NULL");
        return BAD_VALUE;
    }
    if (proxy == 0) {
        ALOGE("camera proxy is NULL");
        return BAD_VALUE;
    }

    mCamera = camera;
    mCameraProxy = proxy;
	CameraParameters newCameraParams(mCamera->getParameters());
	mCaptureFormat =  newCameraParams.getInt("capture-format");
    return OK;
}

status_t AWStagefrightRecorder::setPreviewSurface(const sp<IGraphicBufferProducer> &surface) {
    ALOGV("setPreviewSurface: %p", surface.get());
    mPreviewSurface = surface;

    return OK;
}

status_t AWStagefrightRecorder::setInputSurface(
        const sp<IGraphicBufferConsumer>& surface) {

    return OK;
}

status_t AWStagefrightRecorder::setNextFile(int fd)
{
	status_t err = OK;
	sp<MediaMuxer> mediaMuxer;
	ssize_t videoTrackId = -1;
	ssize_t audioTrackId = -1;
	sp<AMessage> videoFormat;
	sp<AMessage> audioFormat;
	ALOGV("setNextFile fd(%d),outformat(%d)",fd,mOutputFormat);
	switch (mOutputFormat) {
        case OUTPUT_FORMAT_DEFAULT:
        case OUTPUT_FORMAT_THREE_GPP:
        case OUTPUT_FORMAT_MPEG_4:
            mediaMuxer = new MediaMuxer(fd,MediaMuxer::OUTPUT_FORMAT_MPEG_4);
            break;
        case OUTPUT_FORMAT_MPEG2TS:
            mediaMuxer = new MediaMuxer(fd,MediaMuxer::OUTPUT_FORMAT_MPEG2TS);
            break;
        default:
            ALOGE("Unsupported output file format: %d", mOutputFormat);
            break;
    }

	if(V4L2_PIX_FMT_H264 != mCaptureFormat)
	{
		mVideoCodec->getOutputFormat(&videoFormat);
	    videoTrackId = mediaMuxer->addTrack(videoFormat);
	}
	else
	{		
		sp<MetaData> meta = mCameraSource->getFormat();
		meta->setCString(kKeyMIMEType,MEDIA_MIMETYPE_VIDEO_AVC);
		if(mCodecSpecificDataSize <= 0)
		{
			ALOGE("mCodecSpecificDataSize(%d<=0) must be more than 0",mCodecSpecificDataSize);
			return UNKNOWN_ERROR;
		}
		meta->setData(kKeyAVCC, kTypeAVCC,mCodecSpecificData,mCodecSpecificDataSize);
		videoTrackId= mediaMuxer->addTrack(meta);
	}
	
	if(mAudioSourceType != AUDIO_SOURCE_CNT)
	{
		sp<AMessage> audioFormat;
		convertMetaDataToMessage(mAudioTrackMeta, &audioFormat);
		audioTrackId = mediaMuxer->addTrack(audioFormat);
	}
	
	if(mMediaMuxerNext == NULL)
	{
		mMediaMuxerNext = mediaMuxer;
		mVideoTrackIdNext = videoTrackId;
		mAudioTrackIdNext = audioTrackId;
		err = mMediaMuxerNext->start();	
	}
	else if(mMediaMuxer == NULL)
	{
		mMediaMuxer = mediaMuxer;
		mVideoTrackId = videoTrackId;
		mAudioTrackId = audioTrackId;
		err = mMediaMuxer->start();
		
	}
	mMainKeyLock.lock();
	mIsWaitMainKey = true;
	mMainKeyCond.wait(mMainKeyLock);
	mMainKeyLock.unlock();
	if(err != NO_ERROR)
	{
		ALOGE("mediaMuxer start err[%d]",err);
		return err;
	}
	if(mIsNext)
	{
		if(mMediaMuxer != NULL)
		{
			mMediaMuxer->stop();
			mMediaMuxer.clear();
			mMediaMuxer = NULL;
		}
	}
	else
	{
		if(mMediaMuxerNext != NULL)
		{
			mMediaMuxerNext->stop();
			mMediaMuxerNext.clear();
			mMediaMuxerNext = NULL;
		}
	}
	if (mOutputFd >= 0) {
        ::close(mOutputFd);
        mOutputFd = -1;
    }
	mOutputFd = dup(fd);
	return OK;
}

status_t AWStagefrightRecorder::setOutputFile(const char *path) {
    ALOGE("setOutputFile(const char*) must not be called");
    // We don't actually support this at all, as the media_server process
    // no longer has permissions to create files.

    mOutputPath = strdup(path);

    return OK;
}

status_t AWStagefrightRecorder::setOutputFile(int fd, int64_t offset, int64_t length) {
    ALOGV("setOutputFile: %d, %lld, %lld", fd, offset, length);
    // These don't make any sense, do they?
    CHECK_EQ(offset, 0ll);
    CHECK_EQ(length, 0ll);

    if (fd < 0) {
        ALOGE("Invalid file descriptor: %d", fd);
        return -EBADF;
    }

    if (mOutputFd >= 0) {
        ::close(mOutputFd);
    }
    mOutputFd = dup(fd);

    return OK;
}

// Attempt to parse an int64 literal optionally surrounded by whitespace,
// returns true on success, false otherwise.
static bool safe_strtoi64(const char *s, int64_t *val) {
    char *end;

    // It is lame, but according to man page, we have to set errno to 0
    // before calling strtoll().
    errno = 0;
    *val = strtoll(s, &end, 10);

    if (end == s || errno == ERANGE) {
        return false;
    }

    // Skip trailing whitespace
    while (isspace(*end)) {
        ++end;
    }

    // For a successful return, the string must contain nothing but a valid
    // int64 literal optionally surrounded by whitespace.

    return *end == '\0';
}

// Return true if the value is in [0, 0x007FFFFFFF]
static bool safe_strtoi32(const char *s, int32_t *val) {
    int64_t temp;
    if (safe_strtoi64(s, &temp)) {
        if (temp >= 0 && temp <= 0x007FFFFFFF) {
            *val = static_cast<int32_t>(temp);
            return true;
        }
    }
    return false;
}

// Trim both leading and trailing whitespace from the given string.
static void TrimString(String8 *s) {
    size_t num_bytes = s->bytes();
    const char *data = s->string();

    size_t leading_space = 0;
    while (leading_space < num_bytes && isspace(data[leading_space])) {
        ++leading_space;
    }

    size_t i = num_bytes;
    while (i > leading_space && isspace(data[i - 1])) {
        --i;
    }

    s->setTo(String8(&data[leading_space], i - leading_space));
}

status_t AWStagefrightRecorder::setParamAudioSamplingRate(int32_t sampleRate) {
    ALOGV("setParamAudioSamplingRate: %d", sampleRate);
    if (sampleRate <= 0) {
        ALOGE("Invalid audio sampling rate: %d", sampleRate);
        return BAD_VALUE;
    }

    // Additional check on the sample rate will be performed later.
    mSampleRate = sampleRate;
    return OK;
}

status_t AWStagefrightRecorder::setParamAudioNumberOfChannels(int32_t channels) {
    ALOGV("setParamAudioNumberOfChannels: %d", channels);
    if (channels <= 0 || channels >= 3) {
        ALOGE("Invalid number of audio channels: %d", channels);
        return BAD_VALUE;
    }

    // Additional check on the number of channels will be performed later.
    mAudioChannels = channels;
    return OK;
}

status_t AWStagefrightRecorder::setParamAudioEncodingBitRate(int32_t bitRate) {
    ALOGV("setParamAudioEncodingBitRate: %d", bitRate);
    if (bitRate <= 0) {
        ALOGE("Invalid audio encoding bit rate: %d", bitRate);
        return BAD_VALUE;
    }

    // The target bit rate may not be exactly the same as the requested.
    // It depends on many factors, such as rate control, and the bit rate
    // range that a specific encoder supports. The mismatch between the
    // the target and requested bit rate will NOT be treated as an error.
    mAudioBitRate = bitRate;
    return OK;
}

status_t AWStagefrightRecorder::setParamVideoEncodingBitRate(int32_t bitRate) {
    ALOGV("setParamVideoEncodingBitRate: %d", bitRate);
    if (bitRate <= 0) {
        ALOGE("Invalid video encoding bit rate: %d", bitRate);
        return BAD_VALUE;
    }

    // The target bit rate may not be exactly the same as the requested.
    // It depends on many factors, such as rate control, and the bit rate
    // range that a specific encoder supports. The mismatch between the
    // the target and requested bit rate will NOT be treated as an error.
    mVideoBitRate = bitRate;
    return OK;
}

// Always rotate clockwise, and only support 0, 90, 180 and 270 for now.
status_t AWStagefrightRecorder::setParamVideoRotation(int32_t degrees) {
    ALOGV("setParamVideoRotation: %d", degrees);
    if (degrees < 0 || degrees % 90 != 0) {
        ALOGE("Unsupported video rotation angle: %d", degrees);
        return BAD_VALUE;
    }
    mRotationDegrees = degrees % 360;
    return OK;
}

status_t AWStagefrightRecorder::setParamMaxFileDurationUs(int64_t timeUs) {
    ALOGD("setParamMaxFileDurationUs: %lld us", timeUs);

    // This is meant for backward compatibility for MediaRecorder.java
    if (timeUs <= 0) {
        ALOGW("Max file duration is not positive: %lld us. Disabling duration limit.", timeUs);
        timeUs = 0; // Disable the duration limit for zero or negative values.
    } else if (timeUs <= 100000LL) {  // XXX: 100 milli-seconds
        ALOGE("Max file duration is too short: %lld us", timeUs);
        return BAD_VALUE;
    }

    if (timeUs <= 15 * 1000000LL) {
        ALOGW("Target duration (%lld us) too short to be respected", timeUs);
    }
    mMaxFileDurationUs = timeUs;
    return OK;
}

status_t AWStagefrightRecorder::setParamMaxFileSizeBytes(int64_t bytes) {
    ALOGV("setParamMaxFileSizeBytes: %lld bytes", bytes);

    // This is meant for backward compatibility for MediaRecorder.java
    if (bytes <= 0) {
        ALOGW("Max file size is not positive: %lld bytes. "
             "Disabling file size limit.", bytes);
        bytes = 0; // Disable the file size limit for zero or negative values.
    } else if (bytes <= 1024) {  // XXX: 1 kB
        ALOGE("Max file size is too small: %lld bytes", bytes);
        return BAD_VALUE;
    }

    if (bytes <= 100 * 1024) {
        ALOGW("Target file size (%lld bytes) is too small to be respected", bytes);
    }

    mMaxFileSizeBytes = bytes;
    return OK;
}

status_t AWStagefrightRecorder::setParamInterleaveDuration(int32_t durationUs) {
    ALOGV("setParamInterleaveDuration: %d", durationUs);
    if (durationUs <= 500000) {           //  500 ms
        // If interleave duration is too small, it is very inefficient to do
        // interleaving since the metadata overhead will count for a significant
        // portion of the saved contents
        ALOGE("Audio/video interleave duration is too small: %d us", durationUs);
        return BAD_VALUE;
    } else if (durationUs >= 10000000) {  // 10 seconds
        // If interleaving duration is too large, it can cause the recording
        // session to use too much memory since we have to save the output
        // data before we write them out
        ALOGE("Audio/video interleave duration is too large: %d us", durationUs);
        return BAD_VALUE;
    }
    mInterleaveDurationUs = durationUs;
    return OK;
}

// If seconds <  0, only the first frame is I frame, and rest are all P frames
// If seconds == 0, all frames are encoded as I frames. No P frames
// If seconds >  0, it is the time spacing (seconds) between 2 neighboring I frames
status_t AWStagefrightRecorder::setParamVideoIFramesInterval(int32_t seconds) {
    ALOGV("setParamVideoIFramesInterval: %d seconds", seconds);
    mIFramesIntervalSec = seconds;
    return OK;
}

status_t AWStagefrightRecorder::setParam64BitFileOffset(bool use64Bit) {
    ALOGV("setParam64BitFileOffset: %s",
        use64Bit? "use 64 bit file offset": "use 32 bit file offset");
    mUse64BitFileOffset = use64Bit;
    return OK;
}

status_t AWStagefrightRecorder::setParamVideoCameraId(int32_t cameraId) {
    ALOGV("setParamVideoCameraId: %d", cameraId);
    if (cameraId < 0) {
        return BAD_VALUE;
    }
    mCameraId = cameraId;
    return OK;
}

status_t AWStagefrightRecorder::setParamTrackTimeStatus(int64_t timeDurationUs) {
    ALOGV("setParamTrackTimeStatus: %lld", timeDurationUs);
    if (timeDurationUs < 20000) {  // Infeasible if shorter than 20 ms?
        ALOGE("Tracking time duration too short: %lld us", timeDurationUs);
        return BAD_VALUE;
    }
    mTrackEveryTimeDurationUs = timeDurationUs;
    return OK;
}

status_t AWStagefrightRecorder::setParamVideoEncoderProfile(int32_t profile) {
    ALOGV("setParamVideoEncoderProfile: %d", profile);

    // Additional check will be done later when we load the encoder.
    // For now, we are accepting values defined in OpenMAX IL.
    mVideoEncoderProfile = profile;
    return OK;
}

status_t AWStagefrightRecorder::setParamVideoEncoderLevel(int32_t level) {
    ALOGV("setParamVideoEncoderLevel: %d", level);

    // Additional check will be done later when we load the encoder.
    // For now, we are accepting values defined in OpenMAX IL.
    mVideoEncoderLevel = level;
    return OK;
}

status_t AWStagefrightRecorder::setParamMovieTimeScale(int32_t timeScale) {
    ALOGV("setParamMovieTimeScale: %d", timeScale);

    // The range is set to be the same as the audio's time scale range
    // since audio's time scale has a wider range.
    if (timeScale < 600 || timeScale > 96000) {
        ALOGE("Time scale (%d) for movie is out of range [600, 96000]", timeScale);
        return BAD_VALUE;
    }
    mMovieTimeScale = timeScale;
    return OK;
}

status_t AWStagefrightRecorder::setParamVideoTimeScale(int32_t timeScale) {
    ALOGV("setParamVideoTimeScale: %d", timeScale);

    // 60000 is chosen to make sure that each video frame from a 60-fps
    // video has 1000 ticks.
    if (timeScale < 600 || timeScale > 60000) {
        ALOGE("Time scale (%d) for video is out of range [600, 60000]", timeScale);
        return BAD_VALUE;
    }
    mVideoTimeScale = timeScale;
    return OK;
}

status_t AWStagefrightRecorder::setParamAudioTimeScale(int32_t timeScale) {
    ALOGV("setParamAudioTimeScale: %d", timeScale);

    // 96000 Hz is the highest sampling rate support in AAC.
    if (timeScale < 600 || timeScale > 96000) {
        ALOGE("Time scale (%d) for audio is out of range [600, 96000]", timeScale);
        return BAD_VALUE;
    }
    mAudioTimeScale = timeScale;
    return OK;
}

status_t AWStagefrightRecorder::setParamTimeLapseEnable(int32_t timeLapseEnable) {
    ALOGV("setParamTimeLapseEnable: %d", timeLapseEnable);

    if(timeLapseEnable == 0) {
        mCaptureTimeLapse = false;
    } else if (timeLapseEnable == 1) {
        mCaptureTimeLapse = true;
    } else {
        return BAD_VALUE;
    }
    return OK;
}

status_t AWStagefrightRecorder::setParamTimeBetweenTimeLapseFrameCapture(int64_t timeUs) {
    ALOGV("setParamTimeBetweenTimeLapseFrameCapture: %lld us", timeUs);

    // Not allowing time more than a day
    if (timeUs <= 0 || timeUs > 86400*1E6) {
        ALOGE("Time between time lapse frame capture (%lld) is out of range [0, 1 Day]", timeUs);
        return BAD_VALUE;
    }

    mTimeBetweenTimeLapseFrameCaptureUs = timeUs;
    return OK;
}

status_t AWStagefrightRecorder::setParamGeoDataLongitude(
    int64_t longitudex10000) {

    if (longitudex10000 > 1800000 || longitudex10000 < -1800000) {
        return BAD_VALUE;
    }
    mLongitudex10000 = longitudex10000;
    return OK;
}

status_t AWStagefrightRecorder::setParamGeoDataLatitude(
    int64_t latitudex10000) {

    if (latitudex10000 > 900000 || latitudex10000 < -900000) {
        return BAD_VALUE;
    }
    mLatitudex10000 = latitudex10000;
    return OK;
}

status_t AWStagefrightRecorder::setParameter(
        const String8 &key, const String8 &value) {
    ALOGV("setParameter: key (%s) => value (%s)", key.string(), value.string());
    if (key == "max-duration") {
        int64_t max_duration_ms;
        if (safe_strtoi64(value.string(), &max_duration_ms)) {
            return setParamMaxFileDurationUs(1000LL * max_duration_ms);
        }
    } else if (key == "max-filesize") {
        int64_t max_filesize_bytes;
        if (safe_strtoi64(value.string(), &max_filesize_bytes)) {
            return setParamMaxFileSizeBytes(max_filesize_bytes);
        }
    } else if (key == "interleave-duration-us") {
        int32_t durationUs;
        if (safe_strtoi32(value.string(), &durationUs)) {
            return setParamInterleaveDuration(durationUs);
        }
    } else if (key == "param-movie-time-scale") {
        int32_t timeScale;
        if (safe_strtoi32(value.string(), &timeScale)) {
            return setParamMovieTimeScale(timeScale);
        }
    } else if (key == "param-use-64bit-offset") {
        int32_t use64BitOffset;
        if (safe_strtoi32(value.string(), &use64BitOffset)) {
            return setParam64BitFileOffset(use64BitOffset != 0);
        }
    } else if (key == "param-geotag-longitude") {
        int64_t longitudex10000;
        if (safe_strtoi64(value.string(), &longitudex10000)) {
            return setParamGeoDataLongitude(longitudex10000);
        }
    } else if (key == "param-geotag-latitude") {
        int64_t latitudex10000;
        if (safe_strtoi64(value.string(), &latitudex10000)) {
            return setParamGeoDataLatitude(latitudex10000);
        }
    } else if (key == "param-track-time-status") {
        int64_t timeDurationUs;
        if (safe_strtoi64(value.string(), &timeDurationUs)) {
            return setParamTrackTimeStatus(timeDurationUs);
        }
    } else if (key == "audio-param-sampling-rate") {
        int32_t sampling_rate;
        if (safe_strtoi32(value.string(), &sampling_rate)) {
            return setParamAudioSamplingRate(sampling_rate);
        }
    } else if (key == "audio-param-number-of-channels") {
        int32_t number_of_channels;
        if (safe_strtoi32(value.string(), &number_of_channels)) {
            return setParamAudioNumberOfChannels(number_of_channels);
        }
    } else if (key == "audio-param-encoding-bitrate") {
        int32_t audio_bitrate;
        if (safe_strtoi32(value.string(), &audio_bitrate)) {
            return setParamAudioEncodingBitRate(audio_bitrate);
        }
    } else if (key == "audio-param-time-scale") {
        int32_t timeScale;
        if (safe_strtoi32(value.string(), &timeScale)) {
            return setParamAudioTimeScale(timeScale);
        }
    } else if (key == "video-param-encoding-bitrate") {
        int32_t video_bitrate;
        if (safe_strtoi32(value.string(), &video_bitrate)) {
            return setParamVideoEncodingBitRate(video_bitrate);
        }
    } else if (key == "video-param-rotation-angle-degrees") {
        int32_t degrees;
        if (safe_strtoi32(value.string(), &degrees)) {
            return setParamVideoRotation(degrees);
        }
    } else if (key == "video-param-i-frames-interval") {
        int32_t seconds;
        if (safe_strtoi32(value.string(), &seconds)) {
            return setParamVideoIFramesInterval(seconds);
        }
    } else if (key == "video-param-encoder-profile") {
        int32_t profile;
        if (safe_strtoi32(value.string(), &profile)) {
            return setParamVideoEncoderProfile(profile);
        }
    } else if (key == "video-param-encoder-level") {
        int32_t level;
        if (safe_strtoi32(value.string(), &level)) {
            return setParamVideoEncoderLevel(level);
        }
    } else if (key == "video-param-camera-id") {
        int32_t cameraId;
        if (safe_strtoi32(value.string(), &cameraId)) {
            return setParamVideoCameraId(cameraId);
        }
    } else if (key == "video-param-time-scale") {
        int32_t timeScale;
        if (safe_strtoi32(value.string(), &timeScale)) {
            return setParamVideoTimeScale(timeScale);
        }
    } else if (key == "time-lapse-enable") {
        int32_t timeLapseEnable;
        if (safe_strtoi32(value.string(), &timeLapseEnable)) {
            return setParamTimeLapseEnable(timeLapseEnable);
        }
    } else if (key == "time-between-time-lapse-frame-capture") {
        int64_t timeBetweenTimeLapseFrameCaptureMs;
        if (safe_strtoi64(value.string(), &timeBetweenTimeLapseFrameCaptureMs)) {
            return setParamTimeBetweenTimeLapseFrameCapture(
                    1000LL * timeBetweenTimeLapseFrameCaptureMs);
        }
    } else {
        ALOGE("setParameter: failed to find key %s", key.string());
    }
    return BAD_VALUE;
}

status_t AWStagefrightRecorder::setParameters(const String8 &params) {
    ALOGV("setParameters: %s", params.string());
    const char *cparams = params.string();
    const char *key_start = cparams;
    for (;;) {
        const char *equal_pos = strchr(key_start, '=');
        if (equal_pos == NULL) {
            ALOGE("Parameters %s miss a value", cparams);
            return BAD_VALUE;
        }
        String8 key(key_start, equal_pos - key_start);
        TrimString(&key);
        if (key.length() == 0) {
            ALOGE("Parameters %s contains an empty key", cparams);
            return BAD_VALUE;
        }
        const char *value_start = equal_pos + 1;
        const char *semicolon_pos = strchr(value_start, ';');
        String8 value;
        if (semicolon_pos == NULL) {
            value.setTo(value_start);
        } else {
            value.setTo(value_start, semicolon_pos - value_start);
        }
        if (setParameter(key, value) != OK) {
            return BAD_VALUE;
        }
        if (semicolon_pos == NULL) {
            break;  // Reaches the end
        }
        key_start = semicolon_pos + 1;
    }
    return OK;
}

status_t AWStagefrightRecorder::setListener(const sp<IMediaRecorderClient> &listener) {
    mListener = listener;

    return OK;
}

status_t AWStagefrightRecorder::setClientName(const String16& clientName) {
    mClientName = clientName;

    return OK;
}

status_t AWStagefrightRecorder::prepareInternal() {
    ALOGD("prepare mOutputFormat:%d",mOutputFormat);
    if (mOutputFd < 0) {
        ALOGE("Output file descriptor is invalid");
        return INVALID_OPERATION;
    }

    // Get UID here for permission checking
    mClientUid = IPCThreadState::self()->getCallingUid();

    status_t status = OK;

    switch (mOutputFormat) {
        case OUTPUT_FORMAT_DEFAULT:
        case OUTPUT_FORMAT_THREE_GPP:
        case OUTPUT_FORMAT_MPEG_4:
            status = setupMPEG4Recording();
            break;
        case OUTPUT_FORMAT_MPEG2TS:
            status = setupMPEG2TSRecording();
            break;

        default:
            ALOGE("Unsupported output file format: %d", mOutputFormat);
            status = UNKNOWN_ERROR;
            break;
    }

    return status;
}

status_t AWStagefrightRecorder::prepare() {
    if (mVideoSource == VIDEO_SOURCE_CAMERA) {
        return prepareInternal();
    }
    return OK;
}

status_t AWStagefrightRecorder::start() {
	status_t status = OK;
	sp<MetaData> meta = new MetaData;
    if (mOutputFd < 0) {
        ALOGE("Output file descriptor is invalid");
        return INVALID_OPERATION;
    }
	setupMPEG4MetaData(&meta);
	if(V4L2_PIX_FMT_H264 != mCaptureFormat)
	{
		
		mVideoCodec->start();
	}
	else
	{
		sp<MetaData> meta = mCameraSource->getFormat();
		meta->setCString(kKeyMIMEType,MEDIA_MIMETYPE_VIDEO_AVC);
		mVideoTrackId= mMediaMuxer->addTrack(meta);
		if(mAudioSourceType == AUDIO_SOURCE_CNT)
		{
			mMediaMuxer->start();
		}
	}
	
	mCameraSource->start(meta.get());
	if(V4L2_PIX_FMT_H264 != mCaptureFormat)
	{
		mVideoCodec->getInputBuffers(&mVideoInBuffers);
	}
	
	if(mAudioSourceType != AUDIO_SOURCE_CNT)
	{
		mAudioCodec->start();
		mAudioCodec->getInputBuffers(&mAudioInBuffers);
		mAudioSource->start(meta.get());
	}
	if(V4L2_PIX_FMT_H264 != mCaptureFormat)
	{
		status = mVideoCodec->getOutputBuffers(&mVideoOutBuffers);
	    if (status != NO_ERROR) {
	        ALOGE("Unable to get output buffers (err=%d)\n", status);
	        return status;
	    }
	}
	
	mVideoMuxerThread->startThread();
	if(V4L2_PIX_FMT_H264 != mCaptureFormat)
	{
		mVideoEncodeThread->startThread();
	}
	
	if(mAudioSourceType != AUDIO_SOURCE_CNT)
	{
		mAudioCodec->getOutputBuffers(&mAudioOutBuffers);
		mAudioMuxerThread->startThread();
		mAudioEncodeThread->startThread();
	}
    return status;
}

status_t AWStagefrightRecorder::setupMPEG2TSRecording() {
	status_t status;
	sp<AMessage> format;
    mMediaMuxer = new MediaMuxer(mOutputFd,MediaMuxer::OUTPUT_FORMAT_MPEG2TS);
    status = setupVideoEncoder();
    if(status != OK)
    	 return status;
	if(mAudioSourceType != AUDIO_SOURCE_CNT)
	{
		 status = setupAudioEncoder();
	}
	threadInit();
    return status;
}

void AWStagefrightRecorder::clipVideoFrameRate() {
    ALOGV("clipVideoFrameRate: encoder %d", mVideoEncoder);
    
    int minFrameRate = mEncoderProfiles->getVideoEncoderParamByName(
                        "enc.vid.fps.min", mVideoEncoder);
    int maxFrameRate = mEncoderProfiles->getVideoEncoderParamByName(
                        "enc.vid.fps.max", mVideoEncoder);
    if (mFrameRate < minFrameRate && minFrameRate != -1) {
        ALOGW("Intended video encoding frame rate (%d fps) is too small"
             " and will be set to (%d fps)", mFrameRate, minFrameRate);
        mFrameRate = minFrameRate;
    } else if (mFrameRate > maxFrameRate && maxFrameRate != -1) {
        ALOGW("Intended video encoding frame rate (%d fps) is too large"
             " and will be set to (%d fps)", mFrameRate, maxFrameRate);
        mFrameRate = maxFrameRate;
    }
}

void AWStagefrightRecorder::clipVideoBitRate() {
    ALOGV("clipVideoBitRate: encoder %d", mVideoEncoder);
    int minBitRate = mEncoderProfiles->getVideoEncoderParamByName(
                        "enc.vid.bps.min", mVideoEncoder);
    int maxBitRate = mEncoderProfiles->getVideoEncoderParamByName(
                        "enc.vid.bps.max", mVideoEncoder);
    if (mVideoBitRate < minBitRate && minBitRate != -1) {
        ALOGW("Intended video encoding bit rate (%d bps) is too small"
             " and will be set to (%d bps)", mVideoBitRate, minBitRate);
        mVideoBitRate = minBitRate;
    } else if (mVideoBitRate > maxBitRate && maxBitRate != -1) {
        ALOGW("Intended video encoding bit rate (%d bps) is too large"
             " and will be set to (%d bps)", mVideoBitRate, maxBitRate);
        mVideoBitRate = maxBitRate;
    }
}

void AWStagefrightRecorder::clipVideoFrameWidth() {
    ALOGV("clipVideoFrameWidth: encoder %d", mVideoEncoder);
    int minFrameWidth = mEncoderProfiles->getVideoEncoderParamByName(
                        "enc.vid.width.min", mVideoEncoder);
    int maxFrameWidth = mEncoderProfiles->getVideoEncoderParamByName(
                        "enc.vid.width.max", mVideoEncoder);
    if (mVideoWidth < minFrameWidth && minFrameWidth != -1) {
        ALOGW("Intended video encoding frame width (%d) is too small"
             " and will be set to (%d)", mVideoWidth, minFrameWidth);
        mVideoWidth = minFrameWidth;
    } else if (mVideoWidth > maxFrameWidth && maxFrameWidth != -1) {
        ALOGW("Intended video encoding frame width (%d) is too large"
             " and will be set to (%d)", mVideoWidth, maxFrameWidth);
        mVideoWidth = maxFrameWidth;
    }
}

status_t AWStagefrightRecorder::checkVideoEncoderCapabilities(
        bool *supportsCameraSourceMetaDataMode) {
    /* hardware codecs must support camera source meta data mode */
    Vector<CodecCapabilities> codecs;
    OMXClient client;
    CHECK_EQ(client.connect(), (status_t)OK);
    QueryCodecs(
            client.interface(),
            (mVideoEncoder == VIDEO_ENCODER_H263 ? MEDIA_MIMETYPE_VIDEO_H263 :
             mVideoEncoder == VIDEO_ENCODER_MPEG_4_SP ? MEDIA_MIMETYPE_VIDEO_MPEG4 :
             
             mVideoEncoder == VIDEO_ENCODER_H264 ? MEDIA_MIMETYPE_VIDEO_AVC : ""),
            false /* decoder */, true /* hwCodec */, &codecs);
    *supportsCameraSourceMetaDataMode = codecs.size() > 0;
    ALOGD("encoder %s camera source meta-data mode",
            *supportsCameraSourceMetaDataMode ? "supports" : "DOES NOT SUPPORT");

    if (!mCaptureTimeLapse) {
        // Dont clip for time lapse capture as encoder will have enough
        // time to encode because of slow capture rate of time lapse.
        clipVideoBitRate();
        clipVideoFrameRate();
        clipVideoFrameWidth();
        clipVideoFrameHeight();
        setDefaultProfileIfNecessary();
    }
    return OK;
}

// Set to use AVC baseline profile if the encoding parameters matches
// CAMCORDER_QUALITY_LOW profile; this is for the sake of MMS service.
void AWStagefrightRecorder::setDefaultProfileIfNecessary() {
    ALOGV("setDefaultProfileIfNecessary");

    camcorder_quality quality = CAMCORDER_QUALITY_LOW;

    int64_t durationUs   = mEncoderProfiles->getCamcorderProfileParamByName(
                                "duration", mCameraId, quality) * 1000000LL;

    int fileFormat       = mEncoderProfiles->getCamcorderProfileParamByName(
                                "file.format", mCameraId, quality);

    int videoCodec       = mEncoderProfiles->getCamcorderProfileParamByName(
                                "vid.codec", mCameraId, quality);

    int videoBitRate     = mEncoderProfiles->getCamcorderProfileParamByName(
                                "vid.bps", mCameraId, quality);

    int videoFrameRate   = mEncoderProfiles->getCamcorderProfileParamByName(
                                "vid.fps", mCameraId, quality);

    int videoFrameWidth  = mEncoderProfiles->getCamcorderProfileParamByName(
                                "vid.width", mCameraId, quality);

    int videoFrameHeight = mEncoderProfiles->getCamcorderProfileParamByName(
                                "vid.height", mCameraId, quality);

    int audioCodec       = mEncoderProfiles->getCamcorderProfileParamByName(
                                "aud.codec", mCameraId, quality);

    int audioBitRate     = mEncoderProfiles->getCamcorderProfileParamByName(
                                "aud.bps", mCameraId, quality);

    int audioSampleRate  = mEncoderProfiles->getCamcorderProfileParamByName(
                                "aud.hz", mCameraId, quality);

    int audioChannels    = mEncoderProfiles->getCamcorderProfileParamByName(
                                "aud.ch", mCameraId, quality);

    if (durationUs == mMaxFileDurationUs &&
        fileFormat == mOutputFormat &&
        videoCodec == mVideoEncoder &&
        videoBitRate == mVideoBitRate &&
        videoFrameRate == mFrameRate &&
        videoFrameWidth == mVideoWidth &&
        videoFrameHeight == mVideoHeight &&
        audioCodec == mAudioEncoder &&
        audioBitRate == mAudioBitRate &&
        audioSampleRate == mSampleRate &&
        audioChannels == mAudioChannels) {
        if (videoCodec == VIDEO_ENCODER_H264) {
            ALOGI("Force to use AVC baseline profile");
            setParamVideoEncoderProfile(OMX_VIDEO_AVCProfileBaseline);
        }
    }
}

status_t AWStagefrightRecorder::checkAudioEncoderCapabilities() {
    clipAudioBitRate();
    clipAudioSampleRate();
    clipNumberOfAudioChannels();
    return OK;
}

void AWStagefrightRecorder::clipAudioBitRate() {
    ALOGV("clipAudioBitRate: encoder %d", mAudioEncoder);

    int minAudioBitRate =
            mEncoderProfiles->getAudioEncoderParamByName(
                "enc.aud.bps.min", mAudioEncoder);
    if (minAudioBitRate != -1 && mAudioBitRate < minAudioBitRate) {
        ALOGW("Intended audio encoding bit rate (%d) is too small"
            " and will be set to (%d)", mAudioBitRate, minAudioBitRate);
        mAudioBitRate = minAudioBitRate;
    }

    int maxAudioBitRate =
            mEncoderProfiles->getAudioEncoderParamByName(
                "enc.aud.bps.max", mAudioEncoder);
    if (maxAudioBitRate != -1 && mAudioBitRate > maxAudioBitRate) {
        ALOGW("Intended audio encoding bit rate (%d) is too large"
            " and will be set to (%d)", mAudioBitRate, maxAudioBitRate);
        mAudioBitRate = maxAudioBitRate;
    }
}

void AWStagefrightRecorder::clipAudioSampleRate() {
    ALOGV("clipAudioSampleRate: encoder %d", mAudioEncoder);

    int minSampleRate =
            mEncoderProfiles->getAudioEncoderParamByName(
                "enc.aud.hz.min", mAudioEncoder);
    if (minSampleRate != -1 && mSampleRate < minSampleRate) {
        ALOGW("Intended audio sample rate (%d) is too small"
            " and will be set to (%d)", mSampleRate, minSampleRate);
        mSampleRate = minSampleRate;
    }

    int maxSampleRate =
            mEncoderProfiles->getAudioEncoderParamByName(
                "enc.aud.hz.max", mAudioEncoder);
    if (maxSampleRate != -1 && mSampleRate > maxSampleRate) {
        ALOGW("Intended audio sample rate (%d) is too large"
            " and will be set to (%d)", mSampleRate, maxSampleRate);
        mSampleRate = maxSampleRate;
    }
}

void AWStagefrightRecorder::clipNumberOfAudioChannels() {
    ALOGV("clipNumberOfAudioChannels: encoder %d", mAudioEncoder);

    int minChannels =
            mEncoderProfiles->getAudioEncoderParamByName(
                "enc.aud.ch.min", mAudioEncoder);
    if (minChannels != -1 && mAudioChannels < minChannels) {
        ALOGW("Intended number of audio channels (%d) is too small"
            " and will be set to (%d)", mAudioChannels, minChannels);
        mAudioChannels = minChannels;
    }

    int maxChannels =
            mEncoderProfiles->getAudioEncoderParamByName(
                "enc.aud.ch.max", mAudioEncoder);
    if (maxChannels != -1 && mAudioChannels > maxChannels) {
        ALOGW("Intended number of audio channels (%d) is too large"
            " and will be set to (%d)", mAudioChannels, maxChannels);
        mAudioChannels = maxChannels;
    }
}

void AWStagefrightRecorder::clipVideoFrameHeight() {
    ALOGV("clipVideoFrameHeight: encoder %d", mVideoEncoder);
    int minFrameHeight = mEncoderProfiles->getVideoEncoderParamByName(
                        "enc.vid.height.min", mVideoEncoder);
    int maxFrameHeight = mEncoderProfiles->getVideoEncoderParamByName(
                        "enc.vid.height.max", mVideoEncoder);
    if (minFrameHeight != -1 && mVideoHeight < minFrameHeight) {
        ALOGW("Intended video encoding frame height (%d) is too small"
             " and will be set to (%d)", mVideoHeight, minFrameHeight);
        mVideoHeight = minFrameHeight;
    } else if (maxFrameHeight != -1 && mVideoHeight > maxFrameHeight) {
        ALOGW("Intended video encoding frame height (%d) is too large"
             " and will be set to (%d)", mVideoHeight, maxFrameHeight);
        mVideoHeight = maxFrameHeight;
    }
}

// Set up the appropriate MediaSource depending on the chosen option
status_t AWStagefrightRecorder::setupMediaSource(
                      sp<MediaSource> *mediaSource) {
    if (mVideoSource == VIDEO_SOURCE_DEFAULT
            || mVideoSource == VIDEO_SOURCE_CAMERA) {
        sp<CameraSource> cameraSource;
        status_t err = setupCameraSource(&cameraSource);
        if (err != OK) {
            return err;
        }
        *mediaSource = cameraSource;
    }  else {
        return INVALID_OPERATION;
    }
    return OK;
}

// setupSurfaceMediaSource creates a source with the given
// width and height and framerate.
// TODO: This could go in a static function inside SurfaceMediaSource
// similar to that in CameraSource
status_t AWStagefrightRecorder::setupSurfaceMediaSource() {
    status_t err = OK;
    mSurfaceMediaSource = new SurfaceMediaSource(mVideoWidth, mVideoHeight);
    if (mSurfaceMediaSource == NULL) {
        return NO_INIT;
    }

    if (mFrameRate == -1) {
        int32_t frameRate = 0;
        CHECK (mSurfaceMediaSource->getFormat()->findInt32(
                                        kKeyFrameRate, &frameRate));
        ALOGI("Frame rate is not explicitly set. Use the current frame "
             "rate (%d fps)", frameRate);
        mFrameRate = frameRate;
    } else {
        err = mSurfaceMediaSource->setFrameRate(mFrameRate);
    }
    CHECK(mFrameRate != -1);

    mIsMetaDataStoredInVideoBuffers =
        mSurfaceMediaSource->isMetaDataStoredInVideoBuffers();
    return err;
}

status_t AWStagefrightRecorder::setupCameraSource(
        sp<CameraSource> *cameraSource) {
    Size videoSize;
    videoSize.width = mVideoWidth;
    videoSize.height = mVideoHeight;
    if (mCaptureTimeLapse) {
        if (mTimeBetweenTimeLapseFrameCaptureUs < 0) {
            ALOGE("Invalid mTimeBetweenTimeLapseFrameCaptureUs value: %lld",
                mTimeBetweenTimeLapseFrameCaptureUs);
            return BAD_VALUE;
        }

        mCameraSourceTimeLapse = CameraSourceTimeLapse::CreateFromCamera(
                mCamera, mCameraProxy, mCameraId, mClientName, mClientUid,
                videoSize, mFrameRate, mPreviewSurface,
                mTimeBetweenTimeLapseFrameCaptureUs,
                true);
        *cameraSource = mCameraSourceTimeLapse;
    } else {
        *cameraSource = CameraSource::CreateFromCamera(
                mCamera, mCameraProxy, mCameraId, mClientName, mClientUid,
                videoSize, mFrameRate,
                mPreviewSurface, true);
    }
    mCamera.clear();
    mCameraProxy.clear();
    if (*cameraSource == NULL) {
        return UNKNOWN_ERROR;
    }

    if ((*cameraSource)->initCheck() != OK) {
        (*cameraSource).clear();
        *cameraSource = NULL;
        return NO_INIT;
    }

    // When frame rate is not set, the actual frame rate will be set to
    // the current frame rate being used.
    if (mFrameRate == -1) {
        int32_t frameRate = 0;
        CHECK ((*cameraSource)->getFormat()->findInt32(
                    kKeyFrameRate, &frameRate));
        ALOGI("Frame rate is not explicitly set. Use the current frame "
             "rate (%d fps)", frameRate);
        mFrameRate = frameRate;
    }

    CHECK(mFrameRate != -1);

    mIsMetaDataStoredInVideoBuffers =
        (*cameraSource)->isMetaDataStoredInVideoBuffers();

    return OK;
}

status_t AWStagefrightRecorder::setupVideoEncoder()
{
	sp<MediaCodec> videoCodec;
	sp<AMessage> format = new AMessage();
	sp<CameraSource> cameraSource;
    status_t err = setupCameraSource(&cameraSource);
     if (err != OK) {
	 	ALOGE("setupCameraSource err value=%d",err);
         return err;
     }
    mCameraSource = cameraSource;
	if(V4L2_PIX_FMT_H264 == mCaptureFormat)
	{
		return OK;
	}
	videoCodec = MediaCodec::CreateByType(mLooper, MEDIA_MIMETYPE_VIDEO_AVC, true);   
	if (videoCodec == NULL) {
		ALOGE("ERROR: unable to create mVideoEncoder=%d codec instance",mVideoEncoder);
		return UNKNOWN_ERROR;
	}
    format->setString("mime", MEDIA_MIMETYPE_VIDEO_AVC);

    if (mCameraSource != NULL) {
        sp<MetaData> meta = mCameraSource->getFormat();
		int32_t width, height, stride, sliceHeight, colorFormat;
        CHECK(meta->findInt32(kKeyWidth, &width));
        CHECK(meta->findInt32(kKeyHeight, &height));
        CHECK(meta->findInt32(kKeyStride, &stride));
        CHECK(meta->findInt32(kKeySliceHeight, &sliceHeight));
        CHECK(meta->findInt32(kKeyColorFormat, &colorFormat));

		format->setString("mime", "video/avc");
		format->setInt32("color-format", OMX_COLOR_FormatYUV420SemiPlanar);

        format->setInt32("stride", stride);
        format->setInt32("slice-height", sliceHeight);

		/* store-metadata-in-buffers */
		format->setInt32("store-metadata-in-buffers", 1);
		format->setInt32("input-buffer-numbers", 3);
		format->setInt32("input-buffer-size", 1024);

		
		format->setInt32("width", width);
		format->setInt32("height", height);

		
		ALOGD("width,  W: %d, H: %d\n", width, height);
    } else {
        format->setInt32("width", mVideoWidth);
        format->setInt32("height", mVideoHeight);
        format->setInt32("stride", mVideoWidth);
        format->setInt32("slice-height", mVideoWidth);
        format->setInt32("color-format", OMX_COLOR_FormatAndroidOpaque);       
    }

    format->setInt32("bitrate", mVideoBitRate);
    format->setInt32("frame-rate", mFrameRate);
    format->setInt32("i-frame-interval", mIFramesIntervalSec);

	format->setInt32("qp-min", mqpmin);//6
	format->setInt32("qp-max", mqpmax);//45

    if (mVideoTimeScale > 0) {
        format->setInt32("time-scale", mVideoTimeScale);
    }
    if (mVideoEncoderProfile != -1) {
       // format->setInt32("profile", mVideoEncoderProfile);
    }
    if (mVideoEncoderLevel != -1) {
       // format->setInt32("level", mVideoEncoderLevel);
    }

    format->setInt32("priority", 0 /* realtime */);
   

    err = videoCodec->configure(
            format, NULL,
            NULL /* crypto */,
            MediaCodec::CONFIGURE_FLAG_ENCODE /* flags */);

    if (err != NO_ERROR) {
        videoCodec->release();
        videoCodec.clear();
        ALOGE("ERROR: unable to start videoCodec (err=%d)", err);
        return err;
    }
    mVideoCodec = videoCodec;
	return err;
}

status_t AWStagefrightRecorder::setupAudioSource() {
	int32_t sourceSampleRate = mSampleRate;
	sp<AudioSource> audioSource;
    audioSource = new AudioSource(
                mAudioSourceType,
                mOpPackageName,
                sourceSampleRate,
                mAudioChannels,
                mSampleRate);
	mAudioSource = audioSource;
	status_t err = mAudioSource->initCheck();

    if (err != OK) {
        ALOGE("audio source is not initialized");
        return err;
    }
	return OK;
}

status_t AWStagefrightRecorder::setupAudioEncoder() {
	sp<AMessage> format = new AMessage;
	status_t status = setupAudioSource();
	if(status != OK)
		return status;

    switch (mAudioEncoder) {
        case AUDIO_ENCODER_AMR_NB:
        case AUDIO_ENCODER_DEFAULT:
            format->setString("mime", MEDIA_MIMETYPE_AUDIO_AMR_NB);
            break;
        case AUDIO_ENCODER_AMR_WB:
            format->setString("mime", MEDIA_MIMETYPE_AUDIO_AMR_WB);
            break;
        case AUDIO_ENCODER_AAC:
            format->setString("mime", MEDIA_MIMETYPE_AUDIO_AAC);
            format->setInt32("aac-profile", OMX_AUDIO_AACObjectLC);
            break;
        case AUDIO_ENCODER_HE_AAC:
            format->setString("mime", MEDIA_MIMETYPE_AUDIO_AAC);
            format->setInt32("aac-profile", OMX_AUDIO_AACObjectHE);
            break;
        case AUDIO_ENCODER_AAC_ELD:
            format->setString("mime", MEDIA_MIMETYPE_AUDIO_AAC);
            format->setInt32("aac-profile", OMX_AUDIO_AACObjectELD);
            break;

        default:
            ALOGE("Unknown audio encoder: %d", mAudioEncoder);
            return NULL;
    }

    int32_t maxInputSize;
    CHECK(mAudioSource->getFormat()->findInt32(
                kKeyMaxInputSize, &maxInputSize));

    format->setInt32("max-input-size", maxInputSize);
    format->setInt32("channel-count", mAudioChannels);
    format->setInt32("sample-rate", mSampleRate);
    format->setInt32("bitrate", mAudioBitRate);
    if (mAudioTimeScale > 0) {
        format->setInt32("time-scale", mAudioTimeScale);
    }
    format->setInt32("priority", 0 /* realtime */);
	ALOGV("Creating audio codec");
    sp<MediaCodec> audioCodec = MediaCodec::CreateByType(mLooper, MEDIA_MIMETYPE_AUDIO_AAC, true);
    if (audioCodec == NULL) {
        ALOGE("ERROR: unable to create audio codec instance");
        return UNKNOWN_ERROR;
    }

    status = audioCodec->configure(format, NULL, NULL,
            MediaCodec::CONFIGURE_FLAG_ENCODE);
    if (status != OK) {
        audioCodec->release();
        audioCodec.clear();

        ALOGE("ERROR: unable to configure audio codec (err=%d)\n", status);
        return status;
    }
	mAudioCodec = audioCodec;
    return status;
}

status_t AWStagefrightRecorder::setupMPEG4Recording()
{
	status_t status;
	sp<AMessage> format;
    mMediaMuxer = new MediaMuxer(mOutputFd,MediaMuxer::OUTPUT_FORMAT_MPEG_4);
    status = setupVideoEncoder();
    if(status != OK)
    	 return status;
	if(mAudioSourceType != AUDIO_SOURCE_CNT)
	{
		 status = setupAudioEncoder();
	}
	threadInit();
    return status;
}

status_t AWStagefrightRecorder::threadInit()
{
	if(V4L2_PIX_FMT_H264 != mCaptureFormat)
	{
		mVideoEncodeThread = new DoThread(this,THREAD_TYPE_VIDEO_ENCODE);
	}
	
	mVideoMuxerThread = new DoThread(this,THREAD_TYPE_VIDEO_MUXER);
	if(mAudioSourceType != AUDIO_SOURCE_CNT)
	{
		mAudioEncodeThread = new DoThread(this,THREAD_TYPE_AUDIO_ENCODE);
		mAudioMuxerThread = new DoThread(this,THREAD_TYPE_AUDIO_MUXER);
	}
	return OK;
}

status_t AWStagefrightRecorder::threadDestroy()
{
	if(mAudioSourceType != AUDIO_SOURCE_CNT)
	{
		if(mAudioMuxerThread != NULL)
		{
			mAudioMuxerThread->stopThread();
			mAudioMuxerThread.clear();
			mAudioMuxerThread = NULL;
		}
		if(mAudioEncodeThread != NULL)
		{
			mAudioEncodeThread->stopThread();
			mAudioEncodeThread.clear();
			mAudioEncodeThread = NULL;
		}
	}
	if(mVideoMuxerThread != NULL)
	{
		mVideoMuxerThread->stopThread();
		mVideoMuxerThread.clear();
		mVideoMuxerThread = NULL;
	}
	if(mVideoEncodeThread != NULL)
	{
		mVideoEncodeThread->stopThread();
		mVideoEncodeThread.clear();
		mVideoEncodeThread = NULL;
	}
	
	
	return OK;
}
void AWStagefrightRecorder::setupMPEG4MetaData(
        sp<MetaData> *meta) {
    int64_t startTimeUs = systemTime() / 1000;
	int32_t totalBitRate = mVideoBitRate;
	if (mAudioSourceType != AUDIO_SOURCE_CNT)
	{
		totalBitRate += mAudioBitRate;
	}
    (*meta)->setInt64(kKeyTime, startTimeUs);
	ALOGE("setupMPEG4MetaData startTimeUs(%lld)",startTimeUs);
    (*meta)->setInt32(kKeyFileType, mOutputFormat);
    (*meta)->setInt32(kKeyBitRate, totalBitRate);
    (*meta)->setInt32(kKey64BitFileOffset, mUse64BitFileOffset);
    if (mMovieTimeScale > 0) {
        (*meta)->setInt32(kKeyTimeScale, mMovieTimeScale);
    }
    if (mTrackEveryTimeDurationUs > 0) {
        (*meta)->setInt64(kKeyTrackTimeStatus, mTrackEveryTimeDurationUs);
    }
    if (mRotationDegrees != 0) {
        (*meta)->setInt32(kKeyRotation, mRotationDegrees);
    }
}

status_t AWStagefrightRecorder::pause() {
    ALOGV("pause");
    if (mStarted) {
        mStarted = false;

        uint32_t params = 0;
        if (mAudioSourceType != AUDIO_SOURCE_CNT) {
            params |= IMediaPlayerService::kBatteryDataTrackAudio;
        }
        if (mVideoSource != VIDEO_SOURCE_LIST_END) {
            params |= IMediaPlayerService::kBatteryDataTrackVideo;
        }

        addBatteryData(params);
    }


    return OK;
}

status_t AWStagefrightRecorder::stop() {
    ALOGV("stop");
    status_t err = OK;

    if (mCaptureTimeLapse && mCameraSourceTimeLapse != NULL) {
        mCameraSourceTimeLapse->startQuickReadReturns();
        mCameraSourceTimeLapse = NULL;
    }
	threadDestroy();
    if (mMediaMuxer != NULL) {
        err = mMediaMuxer->stop();
        mMediaMuxer.clear();
		mMediaMuxer = NULL;
    }
	if (mMediaMuxerNext != NULL) {
        err = mMediaMuxerNext->stop();
        mMediaMuxerNext.clear();
		mMediaMuxerNext = NULL;
    }
	if(mVideoCodec != NULL)
	{
		mVideoCodec->stop();
		mVideoCodec->release();
		mVideoCodec.clear();
		mVideoCodec = NULL;
	}
	if(mAudioCodec != NULL)
	{
		mAudioCodec->stop();
		mAudioCodec->release();
		mAudioCodec.clear();
		mAudioCodec = NULL;
	}
	if(mCameraSource != NULL)
	{
		mCameraSource->stop();
		mCameraSource.clear();
		mCameraSource = NULL;
	}
	if(mAudioSource != NULL)
	{
		mAudioSource->stop();
		mAudioSource.clear();
		mAudioSource = NULL;
	}
	if(mAudioTrackMeta != NULL)
	{
		mAudioTrackMeta.clear();
		mAudioTrackMeta= NULL;
	}
	if(mCodecSpecificData != NULL)
	{
		free(mCodecSpecificData);
		mCodecSpecificData = NULL;
		mCodecSpecificDataSize = 0;
	}

HWENC_BATTERY:

    if (mOutputFd >= 0) {
        ::close(mOutputFd);
        mOutputFd = -1;
    }
    if (mStarted) {
        mStarted = false;

        uint32_t params = 0;
        if (mAudioSourceType != AUDIO_SOURCE_CNT) {
            params |= IMediaPlayerService::kBatteryDataTrackAudio;
        }
        if (mVideoSource != VIDEO_SOURCE_LIST_END) {
            params |= IMediaPlayerService::kBatteryDataTrackVideo;
        }

        addBatteryData(params);
    }


    return err;
}

status_t AWStagefrightRecorder::close() {
    ALOGV("close");
    stop();

    return OK;
}

status_t AWStagefrightRecorder::reset() {
    ALOGV("reset");
    stop();

    // No audio or video source by default
    mAudioSourceType = AUDIO_SOURCE_CNT;
    mVideoSource = VIDEO_SOURCE_LIST_END;

    // Default parameters
    mOutputFormat  = OUTPUT_FORMAT_THREE_GPP;
    mAudioEncoder  = AUDIO_ENCODER_AMR_NB;
    mVideoEncoder  = VIDEO_ENCODER_H264;
    mVideoWidth    = 176;
    mVideoHeight   = 144;
    mFrameRate     = -1;
    mVideoBitRate  = 192000;
    mSampleRate    = 8000;
    mAudioChannels = 1;
    mAudioBitRate  = 12200;
    mInterleaveDurationUs = 0;
    mIFramesIntervalSec = 1;
    mAudioSourceNode = 0;
    mUse64BitFileOffset = false;
    mMovieTimeScale  = -1;
    mAudioTimeScale  = -1;
    mVideoTimeScale  = -1;
    mCameraId        = 0;
    mStartTimeOffsetMs = -1;
    mVideoEncoderProfile = -1;
    mVideoEncoderLevel   = -1;
    mMaxFileDurationUs = 0;
    mMaxFileSizeBytes = 0;
    mTrackEveryTimeDurationUs = 0;
    mCaptureTimeLapse = false;
    mTimeBetweenTimeLapseFrameCaptureUs = -1;
    mCameraSourceTimeLapse = NULL;
    mIsMetaDataStoredInVideoBuffers = false;
    mEncoderProfiles = MediaProfiles::getInstance();
    mRotationDegrees = 0;
    mLatitudex10000 = -3600000;
    mLongitudex10000 = -3600000;
	mVideoFrameCnt = 0;
	mAudioFrameCnt = 0;
    mOutputFd = -1;
	mAudioTrackId = -1;
	mVideoTrackId = -1;
	mAudioTrackIdNext = -1;
	mVideoTrackIdNext = -1;
	mIsNext = false;
	mIsWaitMainKey = false;
	mAudioTrackMeta = new MetaData;
	mqpmin = 0;
	mqpmax = 0;
	mCaptureFormat = 0;
	mCodecSpecificData = NULL;
	mCodecSpecificDataSize = 0;
	mCodecSPSPPSLength = 0;
    return OK;
}

status_t AWStagefrightRecorder::getMaxAmplitude(int *max) {
    ALOGV("getMaxAmplitude");

    if (max == NULL) {
        ALOGE("Null pointer argument");
        return BAD_VALUE;
    }

    if (mAudioSourceNode != 0) {
        *max = mAudioSourceNode->getMaxAmplitude();
    } else {
        *max = 0;
    }

    return OK;
}

status_t AWStagefrightRecorder::dump(
        int fd, const Vector<String16>& args) const {
    ALOGV("dump");
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;
    snprintf(buffer, SIZE, "   Recorder: %p\n", this);
    snprintf(buffer, SIZE, "   Output file (fd %d):\n", mOutputFd);
    result.append(buffer);
    snprintf(buffer, SIZE, "     File format: %d\n", mOutputFormat);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Max file size (bytes): %lld\n", mMaxFileSizeBytes);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Max file duration (us): %lld\n", mMaxFileDurationUs);
    result.append(buffer);
    snprintf(buffer, SIZE, "     File offset length (bits): %d\n", mUse64BitFileOffset? 64: 32);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Interleave duration (us): %d\n", mInterleaveDurationUs);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Progress notification: %lld us\n", mTrackEveryTimeDurationUs);
    result.append(buffer);
    snprintf(buffer, SIZE, "   Audio\n");
    result.append(buffer);
    snprintf(buffer, SIZE, "     Source: %d\n", mAudioSourceType);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Encoder: %d\n", mAudioEncoder);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Bit rate (bps): %d\n", mAudioBitRate);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Sampling rate (hz): %d\n", mSampleRate);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Number of channels: %d\n", mAudioChannels);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Max amplitude: %d\n", mAudioSourceNode == 0? 0: mAudioSourceNode->getMaxAmplitude());
    result.append(buffer);
    snprintf(buffer, SIZE, "   Video\n");
    result.append(buffer);
    snprintf(buffer, SIZE, "     Source: %d\n", mVideoSource);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Camera Id: %d\n", mCameraId);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Start time offset (ms): %d\n", mStartTimeOffsetMs);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Encoder: %d\n", mVideoEncoder);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Encoder profile: %d\n", mVideoEncoderProfile);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Encoder level: %d\n", mVideoEncoderLevel);
    result.append(buffer);
    snprintf(buffer, SIZE, "     I frames interval (s): %d\n", mIFramesIntervalSec);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Frame size (pixels): %dx%d\n", mVideoWidth, mVideoHeight);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Frame rate (fps): %d\n", mFrameRate);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Bit rate (bps): %d\n", mVideoBitRate);
    result.append(buffer);
    ::write(fd, result.string(), result.size());
    return OK;
}

static void getNalUnitType(uint8_t byte, uint8_t* type) {
    ALOGV("getNalUnitType: %d", byte);

    // nal_unit_type: 5-bit unsigned integer
    *type = (byte & 0x1F);
}

static const uint8_t *findNextStartCode(
        const uint8_t *data, size_t length) {

    ALOGV("findNextStartCode: %p %d", data, length);

    size_t bytesLeft = length;
    while (bytesLeft > 4  &&
            memcmp("\x00\x00\x00\x01", &data[length - bytesLeft], 4)) {
        --bytesLeft;
    }
    if (bytesLeft <= 4) {
        bytesLeft = 0; // Last parameter set
    }
    return &data[length - bytesLeft];
}

static int findNextMainkeyStartCode( const uint8_t *data, size_t length)
{    
	
	if(length < 5)
		return 0;
	//for(int i=0;i<40;i++)
	//{
	//	ALOGE("mainkey[%d]=0x%x",i,data[i]);
	//}
	if(memcmp("\x00\x00\x00\x01\x65", data, 5) == 0)
		return 1;
	return 0;
}

static void StripAWStartcode(MediaBuffer *buffer,int length) {
	size_t totalLen =  buffer->range_length();
    if ((totalLen < 4) || (length > totalLen)) {
        return;
    }
	
    buffer->set_range(
                buffer->range_offset() + length, totalLen - length); 
}

const uint8_t *AWStagefrightRecorder::parseParamSet(
        const uint8_t *data, size_t length, int type, size_t *paramSetLen) {

    ALOGV("parseParamSet");
    CHECK(type == kNalUnitTypeSeqParamSet ||
          type == kNalUnitTypePicParamSet);

    const uint8_t *nextStartCode = findNextStartCode(data, length);
    *paramSetLen = nextStartCode - data;
    if (*paramSetLen == 0) {
        ALOGE("Param set is malformed, since its length is 0");
        return NULL;
    }

    AVCParamSet paramSet(*paramSetLen, data);
    if (type == kNalUnitTypeSeqParamSet) {
        if (*paramSetLen < 4) {
            ALOGE("Seq parameter set malformed");
            return NULL;
        }
        if (mSeqParamSets.empty()) {
            mProfileIdc = data[1];
            mProfileCompatible = data[2];
            mLevelIdc = data[3];
        } else {
            if (mProfileIdc != data[1] ||
                mProfileCompatible != data[2] ||
                mLevelIdc != data[3]) {
                ALOGE("Inconsistent profile/level found in seq parameter sets");
                return NULL;
            }
        }
        mSeqParamSets.push_back(paramSet);
    } else {
        mPicParamSets.push_back(paramSet);
    }
    return nextStartCode;
}

status_t AWStagefrightRecorder::parseAVCCodecSpecificData(
        const uint8_t *data, size_t size) {

    ALOGV("parseAVCCodecSpecificData");
    // Data starts with a start code.
    // SPS and PPS are separated with start codes.
    // Also, SPS must come before PPS
    uint8_t type = kNalUnitTypeSeqParamSet;
    bool gotSps = false;
    bool gotPps = false;
    const uint8_t *tmp = data;
    const uint8_t *nextStartCode = data;
    size_t bytesLeft = size;
    size_t paramSetLen = 0;
    mCodecSpecificDataSize = 0;
    while (bytesLeft > 4 && !memcmp("\x00\x00\x00\x01", tmp, 4)) {
        getNalUnitType(*(tmp + 4), &type);
        if (type == kNalUnitTypeSeqParamSet) {
            if (gotPps) {
                ALOGE("SPS must come before PPS");
                return ERROR_MALFORMED;
            }
            if (!gotSps) {
                gotSps = true;
            }
            nextStartCode = parseParamSet(tmp + 4, bytesLeft - 4, type, &paramSetLen);
        } else if (type == kNalUnitTypePicParamSet) {
            if (!gotSps) {
                ALOGE("SPS must come before PPS");
                return ERROR_MALFORMED;
            }
            if (!gotPps) {
                gotPps = true;
            }
            nextStartCode = parseParamSet(tmp + 4, bytesLeft - 4, type, &paramSetLen);
        } else {
        	if(gotPps && gotSps)
        	{
				break;
        	}
            ALOGE("Only SPS and PPS Nal units are expected");
            return ERROR_MALFORMED;
        }
		
        if (nextStartCode == NULL) {
            return ERROR_MALFORMED;
        }
		
        // Move on to find the next parameter set
        bytesLeft -= nextStartCode - tmp;
		
        tmp = nextStartCode;
        mCodecSpecificDataSize += (2 + paramSetLen);
    }

    {
        // Check on the number of seq parameter sets
        size_t nSeqParamSets = mSeqParamSets.size();
        if (nSeqParamSets == 0) {
            ALOGE("Cound not find sequence parameter set");
            return ERROR_MALFORMED;
        }

        if (nSeqParamSets > 0x1F) {
            ALOGE("Too many seq parameter sets (%d) found", nSeqParamSets);
            return ERROR_MALFORMED;
        }
    }

    {
        // Check on the number of pic parameter sets
        size_t nPicParamSets = mPicParamSets.size();
        if (nPicParamSets == 0) {
            ALOGE("Cound not find picture parameter set");
            return ERROR_MALFORMED;
        }
        if (nPicParamSets > 0xFF) {
            ALOGE("Too many pic parameter sets (%d) found", nPicParamSets);
            return ERROR_MALFORMED;
        }
    }

    return bytesLeft;
}

status_t AWStagefrightRecorder::makeAVCCodecSpecificData(
        const uint8_t *data, size_t size) {

	status_t leftSize = 0;
    if (mCodecSpecificData != NULL) {
        ALOGE("Already have codec specific data");
        return ERROR_MALFORMED;
    }

    if (size < 4) {
        ALOGE("Codec specific data length too short: %d", size);
        return ERROR_MALFORMED;
    }

    if ((leftSize = parseAVCCodecSpecificData(data, size)) < 0) {
        return ERROR_MALFORMED;
    }

    // ISO 14496-15: AVC file format
    mCodecSpecificDataSize += 7;  // 7 more bytes in the header
    mCodecSpecificData = malloc(mCodecSpecificDataSize);
    uint8_t *header = (uint8_t *)mCodecSpecificData;
    header[0] = 1;                     // version
    header[1] = mProfileIdc;           // profile indication
    header[2] = mProfileCompatible;    // profile compatibility
    header[3] = mLevelIdc;

    // 6-bit '111111' followed by 2-bit to lengthSizeMinuusOne
    header[4] = 0xfc | 3;  // length size == 4 bytes

    // 3-bit '111' followed by 5-bit numSequenceParameterSets
    int nSequenceParamSets = mSeqParamSets.size();
    header[5] = 0xe0 | nSequenceParamSets;
    header += 6;
    for (List<AVCParamSet>::iterator it = mSeqParamSets.begin();
         it != mSeqParamSets.end(); ++it) {
        // 16-bit sequence parameter set length
        uint16_t seqParamSetLength = it->mLength;
        header[0] = seqParamSetLength >> 8;
        header[1] = seqParamSetLength & 0xff;

        // SPS NAL unit (sequence parameter length bytes)
        memcpy(&header[2], it->mData, seqParamSetLength);
        header += (2 + seqParamSetLength);
    }

    // 8-bit nPictureParameterSets
    int nPictureParamSets = mPicParamSets.size();
    header[0] = nPictureParamSets;
    header += 1;
    for (List<AVCParamSet>::iterator it = mPicParamSets.begin();
         it != mPicParamSets.end(); ++it) {
        // 16-bit picture parameter set length
        uint16_t picParamSetLength = it->mLength;
        header[0] = picParamSetLength >> 8;
        header[1] = picParamSetLength & 0xff;

        // PPS Nal unit (picture parameter set length bytes)
        memcpy(&header[2], it->mData, picParamSetLength);
        header += (2 + picParamSetLength);
    }

    return leftSize;
}

bool AWStagefrightRecorder::encodeThread(THREAD_TYPE type)
{
	size_t index;
	int64_t timeUs = 0;
	size_t size;
	status_t status;
	MediaBuffer *mediaBuffer;
	status_t err;
	sp<MediaCodec> mediaCodec = NULL;
	Vector<sp<ABuffer> > bufList;
	sp<MediaSource> mediaSource = NULL;
	int frameCnt;
	
	if(type == THREAD_TYPE_VIDEO_ENCODE)
	{
		mediaCodec = mVideoCodec;
		bufList = mVideoInBuffers;
		mediaSource = mCameraSource;
		frameCnt = mVideoFrameCnt;
	}
	else if(type == THREAD_TYPE_AUDIO_ENCODE)
	{
		mediaCodec = mAudioCodec;
		bufList = mAudioInBuffers;
		mediaSource = mAudioSource;
		frameCnt = mAudioFrameCnt;
	}
	else
	{
		ALOGE("encodeThread error type[%d]",type);
		return false;
	}

	err = mediaSource->read(&mediaBuffer);
	if ((err != OK) ||  (frameCnt > DROP_FRAME)) {
		if (err == ERROR_END_OF_STREAM) {
			ALOGD("stream ended, mVideoWidth %d", mVideoWidth);
		} else if(err != OK){
			ALOGE("error %d reading stream.", err);
		}
		else
		{
			ALOGD("encode buffer over %d,mVideoWidth:%d",frameCnt,mVideoWidth);
		}
		if(mediaBuffer != NULL)
		{
			mediaBuffer->release();                
			mediaBuffer = NULL;            
		}            
		return true;
	}
		
	status = mediaCodec->dequeueInputBuffer(&index, ENCODE_TIMEOUT);
	if (status == OK) {
		//ALOGV("filling input mVideoWidth(%d) buffer %d", mVideoWidth,index);
		
		const sp<ABuffer> &buffer = bufList.itemAt(index);
		
		CHECK(mediaBuffer->meta_data()->findInt64(kKeyTime, &timeUs));
		size = mediaBuffer->size();
		memcpy(buffer->data(), mediaBuffer->data(), size);
		err = mediaCodec->queueInputBuffer(
				index,
				0 /* offset */,
				size,
				timeUs,
				0);
		mediaBuffer->release();
		mediaBuffer = NULL;
		if(type == THREAD_TYPE_VIDEO_ENCODE)
		{
			mVideoFrameCnt++;
		}
		else
		{
			mAudioFrameCnt++;
		}
		
	}
	else
	{
		if(mediaBuffer != NULL)
		{
			mediaBuffer->release();                
			mediaBuffer = NULL;            
		}  
		ALOGE("error mVideoCodec->dequeueInputBuffer mVideoWidth(%d)",mVideoWidth);
	}
	return true;
}

status_t AWStagefrightRecorder::getMediaMuxer(THREAD_TYPE type,size_t flags,size_t &trackId,sp<MediaMuxer> &mediaMuxer)
{
	status_t status = OK;
	mMainKeyLock.lock();
	if ((flags & MediaCodec::BUFFER_FLAG_SYNCFRAME) != 0) {
		if(mIsWaitMainKey && (type == THREAD_TYPE_VIDEO_MUXER))
		{
			ALOGD("find next main Key Frame");
			mIsWaitMainKey = false;
			mIsNext = !mIsNext;
			mMainKeyCond.signal();
		}
		
    }
	if(type == THREAD_TYPE_VIDEO_MUXER)
	{
		trackId = mVideoTrackId;
		if(mIsNext)
		{
			trackId= mVideoTrackIdNext;
			mediaMuxer = mMediaMuxerNext;
		}
		else
		{
			mediaMuxer = mMediaMuxer;
		}
	}
	else if(type == THREAD_TYPE_AUDIO_MUXER)
	{
		trackId = mAudioTrackId;
		if(mIsNext)
		{
			trackId= mAudioTrackIdNext;
			mediaMuxer = mMediaMuxerNext;
		}
		else
		{
			mediaMuxer = mMediaMuxer;
		}
	}
	else
	{
		ALOGE("muxerThread error type[%d]",type);
		mMainKeyLock.unlock();
		return UNKNOWN_ERROR;
	}
	mMainKeyLock.unlock();
	return status;
}

status_t AWStagefrightRecorder::muxerRawData()
{
	MediaBuffer *mediaBuffer;
	MediaBuffer *newBuffer;
	VencInputBuffer sInputBuffer;
	status_t err;
	size_t trackId = 0;
	int64_t ptsUsec = 0;
    uint32_t flags = 0;
	sp<MediaMuxer> mediaMuxer = NULL;
	
	err = mCameraSource->read(&mediaBuffer);
	if (err != OK)
	{
		if (err == ERROR_END_OF_STREAM)
		{
			ALOGD("stream ended, mediaBuffer %p", mediaBuffer);
		} 
		else
		{
			ALOGE("error %d reading stream.", err);
		}
		return UNKNOWN_ERROR;
	}
	
	memcpy((void *)&sInputBuffer, (uint8_t *)mediaBuffer->data() + 4, sizeof(VencInputBuffer));
	newBuffer = new MediaBuffer(sInputBuffer.nFlag);
	//ALOGD("sInputBuffer.nFlag=%d",sInputBuffer.nFlag);
	memcpy(newBuffer->data(), sInputBuffer.pAddrVirY,sInputBuffer.nFlag);
	newBuffer->set_range(0, sInputBuffer.nFlag);
   	if(mCodecSpecificDataSize <= 0)
   	{
   		status_t leftSize = makeAVCCodecSpecificData((uint8_t *)(newBuffer->data()),newBuffer->size());
		size_t totalLen = newBuffer->size();
		uint8_t*data = (uint8_t*)(newBuffer->data());
		mCodecSPSPPSLength = totalLen - leftSize;
		if(mCodecSpecificDataSize <= 0)
		{
			ALOGE("line=%d,mCodecSpecificDataSize=%d error!",__LINE__,mCodecSpecificDataSize);
		}
		MediaBuffer *configBuffer = new MediaBuffer(mCodecSpecificDataSize);
		memcpy(configBuffer->data(),mCodecSpecificData,mCodecSpecificDataSize);
		configBuffer->meta_data()->setInt32(kKeyIsCodecConfig,2);
		configBuffer->set_range(configBuffer->range_offset(), mCodecSpecificDataSize);
		err = mMediaMuxer->writeSampleData(configBuffer, mVideoTrackId,0, 0);
		configBuffer->release();
		configBuffer = NULL;
		newBuffer->set_range(newBuffer->range_offset() + mCodecSPSPPSLength, leftSize);
		
		ALOGV("mCodecSpecificDataSize=%d,leftSize=%d",mCodecSpecificDataSize,leftSize);
   	}
	else
	{
		StripAWStartcode(newBuffer,mCodecSPSPPSLength);
	}
	if(findNextMainkeyStartCode((uint8_t *)(newBuffer->data())+newBuffer->range_offset(),newBuffer->range_length()))
	{
		flags = MediaCodec::BUFFER_FLAG_SYNCFRAME;
	}
	getMediaMuxer(THREAD_TYPE_VIDEO_MUXER,flags,trackId,mediaMuxer);
	sp<MetaData> sampleMetaData = mediaBuffer->meta_data();
	sampleMetaData->findInt64(kKeyTime,&ptsUsec);
	if (ptsUsec == 0) {
        ptsUsec = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;
    }
	err = mediaMuxer->writeSampleData(newBuffer, trackId,
                        ptsUsec, flags);
	mediaBuffer->release();
	mediaBuffer = NULL;
	newBuffer->release();
	newBuffer = NULL;
	return err;
}

bool AWStagefrightRecorder::muxerThread(THREAD_TYPE type)
{
	Vector<sp<ABuffer> > buffers;
	size_t bufIndex, offset, size,track_id;
    int64_t ptsUsec;
    uint32_t flags;
	sp<MediaCodec> mediaCodec = NULL;
	sp<MediaMuxer> mediaMuxer = NULL;
	status_t status;
	if(type == THREAD_TYPE_VIDEO_MUXER)
	{
		if(mCaptureFormat == V4L2_PIX_FMT_H264)
		{
			muxerRawData();
			return true;
		}
		mediaCodec = mVideoCodec;
		buffers = mVideoOutBuffers;
	}
	else
	{
		mediaCodec = mAudioCodec;
		buffers = mAudioOutBuffers;
	}

	status = mediaCodec->dequeueOutputBuffer(&bufIndex, &offset, &size, &ptsUsec,
                &flags, ENCODE_TIMEOUT);
    switch (status) {
    case NO_ERROR:
        // got a buffer
       // ALOGV("Got buffer %d,size=%d,type:%d,mIsNext(%d),mVideoWidth(%d)",
       //             bufIndex, size,type,mIsNext,mVideoWidth);
        if ((flags & MediaCodec::BUFFER_FLAG_CODECCONFIG) != 0) {
            // ignore this -- we passed the CSD into MediaMuxer when
            // we got the format change notification
            ALOGV("Got codec config buffer (%u bytes); ignoring", size);
            size = 0;
        }
		getMediaMuxer(type,flags,track_id,mediaMuxer);
        if (size != 0) {

            
            
            if (ptsUsec == 0) {
                ptsUsec = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;
            }
			//ALOGV("mIsNext(%d),mVideoWidth(%d)----------------",mIsNext,mVideoWidth);
			status = mediaMuxer->writeSampleData(buffers[bufIndex], track_id,
		                        ptsUsec, flags);
			
           // ALOGV("Got buffer %d,size=%d,pts=%lld,type:%d,mIsNext(%d),mVideoWidth(%d) end",
           //         bufIndex, size, ptsUsec,type,mIsNext,mVideoWidth);
        }
        status = mediaCodec->releaseOutputBuffer(bufIndex);
		
        if (status != NO_ERROR) {
            ALOGE("Unable to release output buffer (status=%d)", status);
                   
            return status;
        }
		if(type == THREAD_TYPE_VIDEO_MUXER)
		{
			mVideoFrameCnt--;
		}
		else
		{
			mAudioFrameCnt--;
		}
        if ((flags & MediaCodec::BUFFER_FLAG_EOS) != 0) {
			ALOGD("Received end-of-stream");
          
        }
        break;
    case -EAGAIN:                       // INFO_TRY_AGAIN_LATER
        ALOGE("Got -EAGAIN, looping");
        break;
    case INFO_FORMAT_CHANGED:           // INFO_OUTPUT_FORMAT_CHANGED
        {
            // format includes CSD, which we must provide to muxer
            ALOGD("INFO_FORMAT_CHANGED,need addTrack type:%d",type);

            sp<AMessage> newFormat;			
			mediaCodec->getOutputFormat(&newFormat);
			if(type == THREAD_TYPE_VIDEO_MUXER)
			{
				mVideoTrackId= mMediaMuxer->addTrack(newFormat);
			}
			else
			{
				mAudioTrackId= mMediaMuxer->addTrack(newFormat);
				convertMessageToMetaData(newFormat, mAudioTrackMeta);
			}
            if(mAudioSourceType != AUDIO_SOURCE_CNT)
            {
            	if((mVideoTrackId != -1) && (mAudioTrackId != -1))
            	{
            		status = mMediaMuxer->start();
		            if (status != NO_ERROR) {
		                ALOGE("Unable to start muxer (err=%d)\n", status);
		                return status;
		            }
					ALOGD("mMediaMuxer add track ok ,start");
            	}
            }
			else if(mVideoTrackId != -1)
			{
				status = mMediaMuxer->start();
	            if (status != NO_ERROR) {
	                ALOGE("Unable to start muxer (err=%d)\n", status);
	                return status;
	            }
				ALOGD("mMediaMuxer add track ok ,start");
			}
            
        }
        break;
    case INFO_OUTPUT_BUFFERS_CHANGED:   // INFO_OUTPUT_BUFFERS_CHANGED
        // not expected for an encoder; handle it anyway
        
		ALOGD("INFO_OUTPUT_BUFFERS_CHANGED,FUNC:%s,LINE:%d",__FUNCTION__,__LINE__);
		if(type == THREAD_TYPE_VIDEO_MUXER)
		{
			status = mediaCodec->getOutputBuffers(&mVideoOutBuffers);   
		}
		else
		{
			status = mediaCodec->getOutputBuffers(&mAudioOutBuffers); 
		}
		         
		if (status != NO_ERROR) {               
			ALOGE("Unable to get new output buffers (status=%d)\n", status);                
			return status;           
		}
        break;
    case INVALID_OPERATION:
        ALOGE("Request for encoder buffer failed");
        return status;
    default:
        ALOGE("Got weird result from dequeueOutputBuffer");
        return status;
    }
	return true;
}

}  // namespace android

//fengyun add for recorder end 20160601

