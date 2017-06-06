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

#ifndef AWSTAGEFRIGHT_RECORDER_H_

#define AWSTAGEFRIGHT_RECORDER_H_

#include <media/MediaRecorderBase.h>
#include <camera/CameraParameters.h>
#include <utils/String8.h>

#include <system/audio.h>

namespace android {

class Camera;
class ICameraRecordingProxy;
class CameraSource;
class CameraSourceTimeLapse;
struct MediaSource;
struct MediaWriter;
class MetaData;
struct AudioSource;
class MediaProfiles;
class IGraphicBufferConsumer;
class IGraphicBufferProducer;
class SurfaceMediaSource;
struct ALooper;
class MediaCodec;
class MediaMuxer;
class ABuffer;

enum THREAD_TYPE {
    THREAD_TYPE_VIDEO_ENCODE = 0,
	THREAD_TYPE_VIDEO_MUXER = 1,
	THREAD_TYPE_AUDIO_ENCODE = 2,
    THREAD_TYPE_AUDIO_MUXER = 3,
    THREAD_TYPE_CNT = 4,
};


struct AWStagefrightRecorder : public MediaRecorderBase {
    AWStagefrightRecorder(const String16 &opPackageName);
    virtual ~AWStagefrightRecorder();

    virtual status_t init();
    virtual status_t setAudioSource(audio_source_t as);
    virtual status_t setVideoSource(video_source vs);
    virtual status_t setOutputFormat(output_format of);
    virtual status_t setAudioEncoder(audio_encoder ae);
    virtual status_t setVideoEncoder(video_encoder ve);
    virtual status_t setVideoSize(int width, int height);
	virtual status_t setNextFile(int fd);
    virtual status_t setVideoFrameRate(int frames_per_second);
    virtual status_t setCamera(const sp<ICamera>& camera, const sp<ICameraRecordingProxy>& proxy);
    virtual status_t setPreviewSurface(const sp<IGraphicBufferProducer>& surface);
    virtual status_t setOutputFile(const char *path);
    virtual status_t setOutputFile(int fd, int64_t offset, int64_t length);
	virtual status_t setInputSurface(const sp<IGraphicBufferConsumer>& surface);
    virtual status_t setParameters(const String8& params);
    virtual status_t setListener(const sp<IMediaRecorderClient>& listener);
    virtual status_t setClientName(const String16& clientName);
    virtual status_t prepare();
    virtual status_t start();
    virtual status_t pause();
    virtual status_t stop();
    virtual status_t close();
    virtual status_t reset();
    virtual status_t getMaxAmplitude(int *max);
    virtual status_t dump(int fd, const Vector<String16>& args) const;
    // Querying a SurfaceMediaSourcer
    virtual sp<IGraphicBufferProducer> querySurfaceMediaSource() const;
public:
	bool encodeThread(THREAD_TYPE type);
	bool muxerThread(THREAD_TYPE type);
protected:
	class DoThread : public Thread {
        AWStagefrightRecorder*	mRecorder;
		THREAD_TYPE mType;
		bool				mRequestExit;
		Condition mThreadStopCond;
		Mutex mThreadLock;
    public:
        DoThread(AWStagefrightRecorder* recorder,THREAD_TYPE type) :
			Thread(false),
			mRecorder(recorder),
			mType(type),
			mRequestExit(false) {
		}
        void startThread() {
			char name[64] = {0};
			sprintf(name, "DoThread mType[%d]", mType);
			run(name, PRIORITY_URGENT_DISPLAY);
        }
		void stopThread() {
			Mutex::Autolock autoLock(mThreadLock);
			if(!mRequestExit)
			{
				mRequestExit = true;
				mThreadStopCond.wait(mThreadLock);
			}
			
        }
        virtual bool threadLoop() {
			bool ret = false;
			mThreadLock.lock();
			if (mRequestExit) {
				mThreadStopCond.signal();
				mThreadLock.unlock();
				return false;
			}
			mThreadLock.unlock();
			if((mType == THREAD_TYPE_VIDEO_ENCODE) || (mType == THREAD_TYPE_AUDIO_ENCODE))
			{
				ret = mRecorder->encodeThread(mType);
			}
			else if((mType == THREAD_TYPE_VIDEO_MUXER) || (mType == THREAD_TYPE_AUDIO_MUXER))
			{
				ret = mRecorder->muxerThread(mType);
			}
			if(ret == false)
			{
				mThreadLock.lock();
				mRequestExit = true;
				mThreadLock.unlock();
			}
			return ret;
        }
    };

private:
    sp<ICamera> mCamera;
    sp<ICameraRecordingProxy> mCameraProxy;
    sp<IGraphicBufferProducer> mPreviewSurface;
    sp<IMediaRecorderClient> mListener;
    String16 mClientName;
    uid_t mClientUid;
	sp<MediaCodec> mVideoCodec;
	sp<MediaCodec> mAudioCodec;
	sp<DoThread> mAudioEncodeThread;
	sp<DoThread> mVideoEncodeThread;
	sp<DoThread> mVideoMuxerThread;
	sp<DoThread> mAudioMuxerThread;
	ssize_t mVideoTrackId;
	ssize_t mAudioTrackId;
	ssize_t mVideoTrackIdNext;
	ssize_t mAudioTrackIdNext;
	Vector<sp<ABuffer> > mVideoInBuffers;
	Vector<sp<ABuffer> > mAudioInBuffers;
	Vector<sp<ABuffer> > mVideoOutBuffers;
	Vector<sp<ABuffer> > mAudioOutBuffers;
	sp<CameraSource> mCameraSource;
	sp<AudioSource> mAudioSource;
	sp<MediaMuxer> mMediaMuxer;
	sp<MediaMuxer> mMediaMuxerNext;
	sp<MetaData> mAudioTrackMeta;
	Condition mMainKeyCond;
	Mutex mMainKeyLock;
	Mutex mRecorderLock;
	bool mIsNext;
	bool mIsWaitMainKey;
	
	struct AVCParamSet {
        AVCParamSet(uint16_t length, const uint8_t *data)
            : mLength(length), mData(data) {}

        uint16_t mLength;
        const uint8_t *mData;
    };
    List<AVCParamSet> mSeqParamSets;
    List<AVCParamSet> mPicParamSets;
    uint8_t mProfileIdc;
    uint8_t mProfileCompatible;
    uint8_t mLevelIdc;
	void *mCodecSpecificData;
    size_t mCodecSpecificDataSize;
	size_t mCodecSPSPPSLength;
    int mOutputFd;
    char *mOutputPath;
	bool mCaptureTimeLapse;
    sp<AudioSource> mAudioSourceNode;
	int mVideoFrameCnt;
	int mAudioFrameCnt;
    audio_source_t mAudioSourceType;
    video_source mVideoSource;
    output_format mOutputFormat;
    audio_encoder mAudioEncoder;
	video_encoder mVideoEncoder;
    bool mUse64BitFileOffset;
    int32_t mVideoWidth, mVideoHeight;
    int32_t mFrameRate;
    int32_t mVideoBitRate;
    int32_t mAudioBitRate;
    int32_t mAudioChannels;
    int32_t mSampleRate;
    int32_t mInterleaveDurationUs;
    int32_t mIFramesIntervalSec;
    int32_t mCameraId;
    int32_t mVideoEncoderProfile;
    int32_t mVideoEncoderLevel;
    int32_t mMovieTimeScale;
    int32_t mVideoTimeScale;
    int32_t mAudioTimeScale;
    int64_t mMaxFileSizeBytes;
    int64_t mMaxFileDurationUs;
    int64_t mTrackEveryTimeDurationUs;
    int32_t mRotationDegrees;  // Clockwise
    int32_t mLatitudex10000;
    int32_t mLongitudex10000;
    int32_t mStartTimeOffsetMs;
	int32_t mqpmin;
	int32_t mqpmax;
	//fengyun add for packet h264 raw data start
	int32_t mCaptureFormat;
	//fengyun add for packet h264 raw data end
    
    int64_t mTimeBetweenTimeLapseFrameCaptureUs;
    sp<CameraSourceTimeLapse> mCameraSourceTimeLapse;

    String8 mParams;

    bool mIsMetaDataStoredInVideoBuffers;
    MediaProfiles *mEncoderProfiles;
    // Needed when GLFrames are encoded.
    // An <IGraphicBufferProducer> pointer
    // will be sent to the client side using which the
    // frame buffers will be queued and dequeued
    sp<SurfaceMediaSource> mSurfaceMediaSource;
	bool mStarted;
    sp<ALooper> mLooper;
	status_t makeAVCCodecSpecificData(
        const uint8_t *data, size_t size);
	status_t parseAVCCodecSpecificData(
        const uint8_t *data, size_t size);
	const uint8_t *parseParamSet(
        const uint8_t *data, size_t length, int type, size_t *paramSetLen);
	status_t getMediaMuxer(THREAD_TYPE type,size_t flags,size_t &trackId,sp<MediaMuxer> &mediaMuxer);
	status_t muxerRawData();
    void setupMPEG4MetaData(sp<MetaData> *meta);
    status_t startAMRRecording();
    status_t startAACRecording();
    status_t startRawAudioRecording();
    status_t startRTPRecording();
    status_t startMPEG2TSRecording();
    sp<MediaSource> createAudioSource();
    status_t checkVideoEncoderCapabilities(
            bool *supportsCameraSourceMetaDataMode);
    status_t checkAudioEncoderCapabilities();
    // Generic MediaSource set-up. Returns the appropriate
    // source (CameraSource or SurfaceMediaSource)
    // depending on the videosource type
    status_t setupMediaSource(sp<MediaSource> *mediaSource);
    status_t setupCameraSource(sp<CameraSource> *cameraSource);
    // setup the surfacemediasource for the encoder
    status_t setupSurfaceMediaSource();

    status_t setupAudioEncoder();
    status_t setupVideoEncoder();

    // Encoding parameter handling utilities
    status_t setParameter(const String8 &key, const String8 &value);
    status_t setParamAudioEncodingBitRate(int32_t bitRate);
    status_t setParamAudioNumberOfChannels(int32_t channles);
    status_t setParamAudioSamplingRate(int32_t sampleRate);
    status_t setParamAudioTimeScale(int32_t timeScale);
    status_t setParamTimeLapseEnable(int32_t timeLapseEnable);
    status_t setParamTimeBetweenTimeLapseFrameCapture(int64_t timeUs);
    status_t setParamVideoEncodingBitRate(int32_t bitRate);
    status_t setParamVideoIFramesInterval(int32_t seconds);
    status_t setParamVideoEncoderProfile(int32_t profile);
    status_t setParamVideoEncoderLevel(int32_t level);
    status_t setParamVideoCameraId(int32_t cameraId);
    status_t setParamVideoTimeScale(int32_t timeScale);
    status_t setParamVideoRotation(int32_t degrees);
    status_t setParamTrackTimeStatus(int64_t timeDurationUs);
    status_t setParamInterleaveDuration(int32_t durationUs);
    status_t setParam64BitFileOffset(bool use64BitFileOffset);
    status_t setParamMaxFileDurationUs(int64_t timeUs);
    status_t setParamMaxFileSizeBytes(int64_t bytes);
    status_t setParamMovieTimeScale(int32_t timeScale);
    status_t setParamGeoDataLongitude(int64_t longitudex10000);
    status_t setParamGeoDataLatitude(int64_t latitudex10000);
    void clipVideoBitRate();
    void clipVideoFrameRate();
    void clipVideoFrameWidth();
    void clipVideoFrameHeight();
    void clipAudioBitRate();
    void clipAudioSampleRate();
    void clipNumberOfAudioChannels();
    void setDefaultProfileIfNecessary();

	status_t setupAudioSource();
	status_t threadInit();
	status_t threadDestroy();
	status_t prepareInternal();
	status_t setupMPEG4Recording();
	status_t setupMPEG2TSRecording();
    AWStagefrightRecorder(const AWStagefrightRecorder &);
    AWStagefrightRecorder &operator=(const AWStagefrightRecorder &);
};

}  // namespace android

#endif  // STAGEFRIGHT_RECORDER_H_

//fengyun add for recorder end 20160601

