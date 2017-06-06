/*
 * Copyright (C) 2010 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "MPEG2TSWriter"
#include <media/stagefright/foundation/ADebug.h>

#include <media/stagefright/foundation/hexdump.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/MPEG2TSWriter.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/Utils.h>
#include <arpa/inet.h>

#include "include/ESDS.h"

#define SAVE_RAW_STREAM 0

namespace android {

struct MPEG2TSWriter::SourceInfo : public AHandler {
    SourceInfo(const sp<MediaSource> &source);

    void start(const sp<AMessage> &notify);
    void stop();

    unsigned streamType() const;
    unsigned incrementContinuityCounter();

    void readMore();

    enum {
        kNotifyStartFailed,
        kNotifyBuffer,
        kNotifyReachedEOS,
    };

    sp<ABuffer> lastAccessUnit();
    int64_t lastAccessUnitTimeUs();
    void setLastAccessUnit(const sp<ABuffer> &accessUnit);

    void setEOSReceived();
    bool eosReceived() const;

protected:
    virtual void onMessageReceived(const sp<AMessage> &msg);

    virtual ~SourceInfo();

private:
    enum {
        kWhatStart = 'strt',
        kWhatRead  = 'read',
    };

    sp<MediaSource> mSource;
    sp<ALooper> mLooper;
    sp<AMessage> mNotify;

    sp<ABuffer> mAACCodecSpecificData;

    sp<ABuffer> mAACBuffer;

    sp<ABuffer> mLastAccessUnit;
    bool mEOSReceived;

    unsigned mStreamType;
    unsigned mContinuityCounter;

   FILE *mVideoFile;
   FILE *mAudioFile;
   uint8_t mAVCSpecificData[128];
   size_t  mAVCSpecificDataLen;
   bool    mFirstFrame;

    void extractCodecSpecificData();

    bool appendAACFrames(MediaBuffer *buffer);
    bool flushAACFrames();

    void postAVCFrame(MediaBuffer *buffer);

    DISALLOW_EVIL_CONSTRUCTORS(SourceInfo);
};

MPEG2TSWriter::SourceInfo::SourceInfo(const sp<MediaSource> &source)
    : mSource(source),
      mLooper(new ALooper),
      mEOSReceived(false),
      mStreamType(0),
      mContinuityCounter(0),
      mVideoFile(NULL),
      mAudioFile(NULL),
      mFirstFrame(true)
       {
    mLooper->setName("MPEG2TSWriter source");

    sp<MetaData> meta = mSource->getFormat();
    const char *mime;
    CHECK(meta->findCString(kKeyMIMEType, &mime));

    if (!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_AAC)) {
        mStreamType = 0x0f;
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_VIDEO_AVC)) {
        mStreamType = 0x1b;
    } else {
        TRESPASS();
    }
}

MPEG2TSWriter::SourceInfo::~SourceInfo() {
#if (SAVE_RAW_STREAM)
   if (mVideoFile)
       fclose(mVideoFile);
   if (mAudioFile)
       fclose(mAudioFile);
#endif
}

unsigned MPEG2TSWriter::SourceInfo::streamType() const {
    return mStreamType;
}

unsigned MPEG2TSWriter::SourceInfo::incrementContinuityCounter() {
   if (mFirstFrame) {
           mFirstFrame = false;
       } else {
           if (++mContinuityCounter == 16) {
           mContinuityCounter = 0;
       }
   }

    return mContinuityCounter;
}

void MPEG2TSWriter::SourceInfo::start(const sp<AMessage> &notify) {
    mLooper->registerHandler(this);
    mLooper->start();

    mNotify = notify;

    status_t err = mSource->start();
    if (err != OK) {
        sp<AMessage> notify = mNotify->dup();
        notify->setInt32("what", kNotifyStartFailed);
        notify->post();
        return;
    }

    extractCodecSpecificData();

    readMore();
    //(new AMessage(kWhatStart, id()))->post();
}

void MPEG2TSWriter::SourceInfo::stop() {
	status_t err = mSource->stop();
	if (err != OK) {
	    ALOGE("stop ts source error\n");
	}
	while (!mEOSReceived) {
		ALOGV("aw_ts stop source ,but eos not received\n");
		usleep(1000);
	}
	mLooper->unregisterHandler(id());
	mLooper->stop();
}

void MPEG2TSWriter::SourceInfo::extractCodecSpecificData() {
    sp<MetaData> meta = mSource->getFormat();

    const char *mime;
    CHECK(meta->findCString(kKeyMIMEType, &mime));

    if (!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_AAC)) {
        uint32_t type;
        const void *data;
        size_t size;
        if (!meta->findData(kKeyESDS, &type, &data, &size)) {
            // Codec specific data better be in the first data buffer.
            return;
        }

        ESDS esds((const char *)data, size);
        CHECK_EQ(esds.InitCheck(), (status_t)OK);

        const uint8_t *codec_specific_data;
        size_t codec_specific_data_size;
        esds.getCodecSpecificInfo(
                (const void **)&codec_specific_data, &codec_specific_data_size);

        CHECK_GE(codec_specific_data_size, 2u);

        mAACCodecSpecificData = new ABuffer(codec_specific_data_size);

        memcpy(mAACCodecSpecificData->data(), codec_specific_data,
               codec_specific_data_size);

   #if (SAVE_RAW_STREAM)
       mAudioFile = fopen("/sdcard/raw.aac", "wb");
       if (mAudioFile == NULL) {
           ALOGE("open mAudioFile error\n");
           return;
       }
   #endif

        return;
    }

    if (strcasecmp(mime, MEDIA_MIMETYPE_VIDEO_AVC)) {
        return;
    }

    uint32_t type;
    const void *data;
    size_t size;
    if (!meta->findData(kKeyAVCC, &type, &data, &size)) {
        // Codec specific data better be part of the data stream then.
        return;
    }

    sp<ABuffer> out = new ABuffer(1024);
    out->setRange(0, 0);

    const uint8_t *ptr = (const uint8_t *)data;

    size_t numSeqParameterSets = ptr[5] & 31;

    ptr += 6;
    size -= 6;

    for (size_t i = 0; i < numSeqParameterSets; ++i) {
        CHECK(size >= 2);
        size_t length = U16_AT(ptr);

        ptr += 2;
        size -= 2;

        CHECK(size >= length);

        CHECK_LE(out->size() + 4 + length, out->capacity());
        memcpy(out->data() + out->size(), "\x00\x00\x00\x01", 4);
        memcpy(out->data() + out->size() + 4, ptr, length);
        out->setRange(0, out->size() + length + 4);

        ptr += length;
        size -= length;
    }

    CHECK(size >= 1);
    size_t numPictureParameterSets = *ptr;
    ++ptr;
    --size;

    for (size_t i = 0; i < numPictureParameterSets; ++i) {
        CHECK(size >= 2);
        size_t length = U16_AT(ptr);

        ptr += 2;
        size -= 2;

        CHECK(size >= length);

        CHECK_LE(out->size() + 4 + length, out->capacity());
        memcpy(out->data() + out->size(), "\x00\x00\x00\x01", 4);
        memcpy(out->data() + out->size() + 4, ptr, length);
        out->setRange(0, out->size() + length + 4);

        ptr += length;
        size -= length;
    }

#if (SAVE_RAW_STREAM)
   mVideoFile = fopen("/sdcard/raw.h264", "wb");
   if (mVideoFile == NULL) {
       ALOGE("open mVideoFile \n");
       return;
   }
   fwrite(out->data(), 1, out->size(), mVideoFile);
#endif

    out->meta()->setInt64("timeUs", 0ll);

   memset(mAVCSpecificData, 0, sizeof(mAVCSpecificData));
   memcpy(mAVCSpecificData, out->data(), out->size());
   mAVCSpecificDataLen = out->size();

    sp<AMessage> notify = mNotify->dup();
    notify->setInt32("what", kNotifyBuffer);
    notify->setBuffer("buffer", out);
    notify->setInt32("oob", true);
    notify->post();
}

void MPEG2TSWriter::SourceInfo::postAVCFrame(MediaBuffer *buffer) {
    sp<AMessage> notify = mNotify->dup();
    notify->setInt32("what", kNotifyBuffer);

   int32_t isSync = false;
   int32_t copyLen = 0;
   int32_t copyOffset = 0;
   int32_t isCodecConfig =0;
   int32_t prefixLen = 6;
    uint8_t prefixData[6] = {0x00, 0x00, 0x00, 0x01, 0x09, 0x30};
	if((buffer->meta_data()->findInt32(kKeyIsCodecConfig, &isCodecConfig)) && isCodecConfig)
	{
    size_t size = buffer->range_length();;
    ALOGD("kKeyIsCodecConfig:%d,mAVCSpecificDataLen=%d",isCodecConfig,size);

    sp<ABuffer> out = new ABuffer(1024);
    out->setRange(0, 0);

    const uint8_t *ptr = (const uint8_t *)buffer->data();
    size_t numSeqParameterSets = ptr[5] & 31;

    ptr += 6;
    size -= 6;
	ALOGD("numSeqParameterSets=%d",numSeqParameterSets);
    for (size_t i = 0; i < numSeqParameterSets; ++i) {
        CHECK(size >= 2);
        size_t length = U16_AT(ptr);

        ptr += 2;
        size -= 2;

        CHECK(size >= length);

        CHECK_LE(out->size() + 4 + length, out->capacity());
        memcpy(out->data() + out->size(), "\x00\x00\x00\x01", 4);
        memcpy(out->data() + out->size() + 4, ptr, length);
        out->setRange(0, out->size() + length + 4);

        ptr += length;
        size -= length;
    }

    CHECK(size >= 1);
    size_t numPictureParameterSets = *ptr;
    ++ptr;
    --size;

    for (size_t i = 0; i < numPictureParameterSets; ++i) {
        CHECK(size >= 2);
        size_t length = U16_AT(ptr);

        ptr += 2;
        size -= 2;

        CHECK(size >= length);

        CHECK_LE(out->size() + 4 + length, out->capacity());
        memcpy(out->data() + out->size(), "\x00\x00\x00\x01", 4);
        memcpy(out->data() + out->size() + 4, ptr, length);
        out->setRange(0, out->size() + length + 4);

        ptr += length;
        size -= length;
    }
    out->meta()->setInt64("timeUs", 0ll);
   memset(mAVCSpecificData, 0, sizeof(mAVCSpecificData));
   memcpy(mAVCSpecificData, out->data(), out->size());
   mAVCSpecificDataLen = out->size();
#if (SAVE_RAW_STREAM)
	  mVideoFile = fopen("/sdcard/raw.h264", "wb");
	  if (mVideoFile == NULL) {
		  ALOGE("open mVideoFile error\n");
		  readMore();
		  return;
	  }
	  fwrite(out->data(), 1, out->size(), mVideoFile);
#endif
	ALOGD("real mAVCSpecificDataLen=%d",mAVCSpecificDataLen);
	readMore();
	return;
	}
    if (buffer->meta_data()->findInt32(kKeyIsSyncFrame, &isSync)
            && isSync != 0) {
        copyLen = buffer->range_length() + mAVCSpecificDataLen + prefixLen;
       copyOffset = mAVCSpecificDataLen + prefixLen;
    } else {
       copyLen = buffer->range_length() + prefixLen;
       copyOffset = prefixLen;
    }

   sp<ABuffer> copy = new ABuffer(copyLen);
   memcpy(copy->data(), prefixData, prefixLen);
   if (isSync) {
       copy->meta()->setInt32("isSync", true);
       memcpy(copy->data() + prefixLen, mAVCSpecificData, mAVCSpecificDataLen);
   }
    memcpy(copy->data() + copyOffset,
           (const uint8_t *)buffer->data()
            + buffer->range_offset(),
           buffer->range_length());

#if (SAVE_RAW_STREAM)
   fwrite((void *)((unsigned char *)buffer->data() + buffer->range_offset()), 1, buffer->range_length(), mVideoFile);
#endif

    int64_t timeUs;
    CHECK(buffer->meta_data()->findInt64(kKeyTime, &timeUs));
    copy->meta()->setInt64("timeUs", timeUs);

    notify->setBuffer("buffer", copy);
    notify->post();
}

bool MPEG2TSWriter::SourceInfo::appendAACFrames(MediaBuffer *buffer) {
    bool accessUnitPosted = false;

   int64_t timeUs;
    CHECK(buffer->meta_data()->findInt64(kKeyTime, &timeUs));
    if (mAACBuffer == NULL) {
        size_t alloc = 7 + buffer->range_length();
        mAACBuffer = new ABuffer(alloc);
        mAACBuffer->meta()->setInt32("isSync", true);
    }
       mAACBuffer->meta()->setInt64("timeUs", timeUs);

    const uint8_t *codec_specific_data = mAACCodecSpecificData->data();
    unsigned profile = (codec_specific_data[0] >> 3) - 1;

    unsigned sampling_freq_index =
        ((codec_specific_data[0] & 7) << 1)
        | (codec_specific_data[1] >> 7);

    unsigned channel_configuration =
        (codec_specific_data[1] >> 3) & 0x0f;

    uint8_t *ptr = mAACBuffer->data();

    const uint32_t aac_frame_length = buffer->range_length() + 7;

    *ptr++ = 0xff;
    *ptr++ = 0xf1;  // b11110001, ID=0, layer=0, protection_absent=1

    *ptr++ =
        profile << 6
        | sampling_freq_index << 2
        | ((channel_configuration >> 2) & 1);  // private_bit=0

    // original_copy=0, home=0, copyright_id_bit=0, copyright_id_start=0
    *ptr++ =
        (channel_configuration & 3) << 6
        | aac_frame_length >> 11;
    *ptr++ = (aac_frame_length >> 3) & 0xff;
    *ptr++ = (aac_frame_length & 7) << 5;

    // adts_buffer_fullness=0, number_of_raw_data_blocks_in_frame=0
    *ptr++ = 0;

    memcpy(ptr,
           (const uint8_t *)buffer->data() + buffer->range_offset(),
           buffer->range_length());

   #if (SAVE_RAW_STREAM)
       fwrite(mAACBuffer->data(), 1, buffer->range_length() + 7, mAudioFile);
   #endif

   accessUnitPosted = flushAACFrames();
    return accessUnitPosted;
}

bool MPEG2TSWriter::SourceInfo::flushAACFrames() {
    if (mAACBuffer == NULL) {
        return false;
    }

    sp<AMessage> notify = mNotify->dup();
    notify->setInt32("what", kNotifyBuffer);
    notify->setBuffer("buffer", mAACBuffer);
    notify->post();

    mAACBuffer.clear();

    return true;
}

void MPEG2TSWriter::SourceInfo::readMore() {
    (new AMessage(kWhatRead, this))->post();
}

void MPEG2TSWriter::SourceInfo::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatStart:
        {
            status_t err = mSource->start();
            if (err != OK) {
                sp<AMessage> notify = mNotify->dup();
                notify->setInt32("what", kNotifyStartFailed);
                notify->post();
                break;
            }

            extractCodecSpecificData();

            readMore();
            break;
        }

        case kWhatRead:
        {
            MediaBuffer *buffer;
            status_t err = mSource->read(&buffer);

            if (err != OK && err != INFO_FORMAT_CHANGED) {
                if (mStreamType == 0x0f) {
                    flushAACFrames();
                }

                sp<AMessage> notify = mNotify->dup();
                notify->setInt32("what", kNotifyReachedEOS);
                notify->setInt32("status", err);
                notify->post();
                break;
            }

            if (err == OK) {
                if (mStreamType == 0x0f && mAACCodecSpecificData == NULL) {
                    // The first buffer contains codec specific data.

                    CHECK_GE(buffer->range_length(), 2u);

                    mAACCodecSpecificData = new ABuffer(buffer->range_length());

                    memcpy(mAACCodecSpecificData->data(),
                           (const uint8_t *)buffer->data()
                            + buffer->range_offset(),
                           buffer->range_length());
                } else if (buffer->range_length() > 0) {
                    if (mStreamType == 0x0f) {
                        if (!appendAACFrames(buffer)) {
                            msg->post();
                        }
                    } else {
                        postAVCFrame(buffer);
                    }
                } else {
                    readMore();
                }

                buffer->release();
                buffer = NULL;
            }

            // Do not read more data until told to.
            break;
        }

        default:
            TRESPASS();
    }
}

sp<ABuffer> MPEG2TSWriter::SourceInfo::lastAccessUnit() {
    return mLastAccessUnit;
}

void MPEG2TSWriter::SourceInfo::setLastAccessUnit(
        const sp<ABuffer> &accessUnit) {
    mLastAccessUnit = accessUnit;
}

int64_t MPEG2TSWriter::SourceInfo::lastAccessUnitTimeUs() {
    if (mLastAccessUnit == NULL) {
        return -1;
    }

    int64_t timeUs;
    CHECK(mLastAccessUnit->meta()->findInt64("timeUs", &timeUs));

    return timeUs;
}

void MPEG2TSWriter::SourceInfo::setEOSReceived() {
    CHECK(!mEOSReceived);
    mEOSReceived = true;
}

bool MPEG2TSWriter::SourceInfo::eosReceived() const {
    return mEOSReceived;
}

////////////////////////////////////////////////////////////////////////////////

MPEG2TSWriter::MPEG2TSWriter(int fd)
    : mFile(fdopen(dup(fd), "wb")),
      mWriteCookie(NULL),
      mWriteFunc(NULL),
      mStarted(false),
      mNumSourcesDone(0),
      mNumTSPacketsWritten(0),
      mNumTSPacketsBeforeMeta(0),
      mPATContinuityCounter(0),
      mPMTContinuityCounter(0) {
    init();
}

MPEG2TSWriter::MPEG2TSWriter(const char *filename)
    : mFile(fopen(filename, "wb")),
      mWriteCookie(NULL),
      mWriteFunc(NULL),
      mStarted(false),
      mNumSourcesDone(0),
      mNumTSPacketsWritten(0),
      mNumTSPacketsBeforeMeta(0),
      mPATContinuityCounter(0),
      mPMTContinuityCounter(0) {
    init();
}

MPEG2TSWriter::MPEG2TSWriter(
        void *cookie,
        ssize_t (*write)(void *cookie, const void *data, size_t size))
    : mFile(NULL),
      mWriteCookie(cookie),
      mWriteFunc(write),
      mStarted(false),
      mNumSourcesDone(0),
      mNumTSPacketsWritten(0),
      mNumTSPacketsBeforeMeta(0),
      mPATContinuityCounter(0),
      mPMTContinuityCounter(0) {
    init();
}

void MPEG2TSWriter::init() {
    CHECK(mFile != NULL || mWriteFunc != NULL);

    initCrcTable();

    mLooper = new ALooper;
    mLooper->setName("MPEG2TSWriter");

    mReflector = new AHandlerReflector<MPEG2TSWriter>(this);

    mLooper->registerHandler(mReflector);
    mLooper->start();
}

MPEG2TSWriter::~MPEG2TSWriter() {
    if (mStarted) {
        reset();
    }

    mLooper->unregisterHandler(mReflector->id());
    mLooper->stop();

    if (mFile != NULL) {
        fclose(mFile);
        mFile = NULL;
    }
	ALOGV("aw_ts end ~MPEG2TSWriter\n");
}

status_t MPEG2TSWriter::addSource(const sp<MediaSource> &source) {
    CHECK(!mStarted);

    sp<MetaData> meta = source->getFormat();
    const char *mime;
    CHECK(meta->findCString(kKeyMIMEType, &mime));

    if (strcasecmp(mime, MEDIA_MIMETYPE_VIDEO_AVC)
            && strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_AAC)) {
        return ERROR_UNSUPPORTED;
    }

    sp<SourceInfo> info = new SourceInfo(source);

    mSources.push(info);

    return OK;
}

status_t MPEG2TSWriter::start(MetaData * /* param */) {
    CHECK(!mStarted);

    mStarted = true;
    mNumSourcesDone = 0;
    mNumTSPacketsWritten = 0;
    mNumTSPacketsBeforeMeta = 0;

   //config the AwCache
   mAwCache.mNeedCache = true;
   mAwCache.mAlign = 4096;
   mAwCache.mCacheSize = 128*1024;
   mAwCache.mValidCacheSize = 0;
   mAwCache.mCacheData = NULL;
   status_t ret = posix_memalign((void **)&(mAwCache.mCacheData), mAwCache.mAlign, mAwCache.mCacheSize);
    if(ret != OK)
    {
        ALOGE("(f:%s, l:%d) fatal error! posix_memalign fail!", __FUNCTION__, __LINE__);
        return -1;
    }

    for (size_t i = 0; i < mSources.size(); ++i) {
        sp<AMessage> notify =
            new AMessage(kWhatSourceNotify, mReflector);

        notify->setInt32("source-index", i);

        mSources.editItemAt(i)->start(notify);
    }

    return OK;
}

status_t MPEG2TSWriter::reset() {
    CHECK(mStarted);

    for (size_t i = 0; i < mSources.size(); ++i) {
        mSources.editItemAt(i)->stop();
    }

	if(mAwCache.mCacheData && mAwCache.mNeedCache)
	{
		if (mAwCache.mValidCacheSize > 0)
			fwrite(mAwCache.mCacheData, 1, mAwCache.mValidCacheSize, mFile);

		free(mAwCache.mCacheData);
	}

    mStarted = false;

    return OK;
}

status_t MPEG2TSWriter::pause() {
    CHECK(mStarted);

    return OK;
}

bool MPEG2TSWriter::reachedEOS() {
    return !mStarted || (mNumSourcesDone == mSources.size() ? true : false);
}

status_t MPEG2TSWriter::dump(
        int /* fd */, const Vector<String16> & /* args */) {
    return OK;
}

void MPEG2TSWriter::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatSourceNotify:
        {
            int32_t sourceIndex;
            CHECK(msg->findInt32("source-index", &sourceIndex));

            int32_t what;
            CHECK(msg->findInt32("what", &what));

            if (what == SourceInfo::kNotifyReachedEOS
                    || what == SourceInfo::kNotifyStartFailed) {
                sp<SourceInfo> source = mSources.editItemAt(sourceIndex);
                source->setEOSReceived();

                sp<ABuffer> buffer = source->lastAccessUnit();
                source->setLastAccessUnit(NULL);

                if (buffer != NULL) {
                    writeTS();
                    writeAccessUnit(sourceIndex, buffer);
                }

                ++mNumSourcesDone;
            } else if (what == SourceInfo::kNotifyBuffer) {
                sp<ABuffer> buffer;
                CHECK(msg->findBuffer("buffer", &buffer));

                int32_t oob;
                if (msg->findInt32("oob", &oob) && oob) {
                    // This is codec specific data delivered out of band.
                    // It can be written out immediately.
                    if (0) {
                       writeTS();
                       writeAccessUnit(sourceIndex, buffer);
                    }
                    break;
                }

                // We don't just write out data as we receive it from
                // the various sources. That would essentially write them
                // out in random order (as the thread scheduler determines
                // how the messages are dispatched).
                // Instead we gather an access unit for all tracks and
                // write out the one with the smallest timestamp, then
                // request more data for the written out track.
                // Rinse, repeat.
                // If we don't have data on any track we don't write
                // anything just yet.

                sp<SourceInfo> source = mSources.editItemAt(sourceIndex);

                CHECK(source->lastAccessUnit() == NULL);
                source->setLastAccessUnit(buffer);

                ALOGV("lastAccessUnitTimeUs[%d] = %.2f secs",
                     sourceIndex, source->lastAccessUnitTimeUs() / 1E6);

                if (source->eosReceived()) {
                        break;
                     }

                buffer = source->lastAccessUnit();
                source->setLastAccessUnit(NULL);

                writeTS();
                writeAccessUnit(sourceIndex, buffer);

                source->readMore();
            }
            break;
        }

        default:
            TRESPASS();
    }
}

void MPEG2TSWriter::writeProgramAssociationTable() {
    // 0x47
    // transport_error_indicator = b0
    // payload_unit_start_indicator = b1
    // transport_priority = b0
    // PID = b0000000000000 (13 bits)
    // transport_scrambling_control = b00
    // adaptation_field_control = b01 (no adaptation field, payload only)
    // continuity_counter = b????
    // skip = 0x00
    // --- payload follows
    // table_id = 0x00
    // section_syntax_indicator = b1
    // must_be_zero = b0
    // reserved = b11
    // section_length = 0x00d
    // transport_stream_id = 0x0000
    // reserved = b11
    // version_number = b00001
    // current_next_indicator = b1
    // section_number = 0x00
    // last_section_number = 0x00
    //   one program follows:
    //   program_number = 0x0001
    //   reserved = b111
    //   program_map_PID = 0x01e0 (13 bits!)
    // CRC = 0x????????

    static const uint8_t kData[] = {
        0x47,
        0x40, 0x00, 0x10, 0x00,  // b0100 0000 0000 0000 0001 ???? 0000 0000
        0x00, 0xb0, 0x0d, 0x00,  // b0000 0000 1011 0000 0000 1101 0000 0000
        0x00, 0xc3, 0x00, 0x00,  // b0000 0000 1100 0011 0000 0000 0000 0000
        0x00, 0x01, 0xe1, 0xe0,  // b0000 0000 0000 0001 1110 0001 1110 0000
        0x00, 0x00, 0x00, 0x00   // b???? ???? ???? ???? ???? ???? ???? ????
    };

    sp<ABuffer> buffer = new ABuffer(188);
    memset(buffer->data(), 0xff, buffer->size());
    memcpy(buffer->data(), kData, sizeof(kData));

    if (mPATContinuityCounter == 16) {
        mPATContinuityCounter = 0;
    }
    buffer->data()[3] |= mPATContinuityCounter;
   mPATContinuityCounter++;

    uint32_t crc = htonl(crc32(&buffer->data()[5], 12));
    memcpy(&buffer->data()[17], &crc, sizeof(crc));

    CHECK_EQ(internalWrite(buffer->data(), buffer->size()), buffer->size());
}

void MPEG2TSWriter::writeProgramMap() {
    // 0x47
    // transport_error_indicator = b0
    // payload_unit_start_indicator = b1
    // transport_priority = b0
    // PID = b0 0001 1110 0000 (13 bits) [0x1e0]
    // transport_scrambling_control = b00
    // adaptation_field_control = b01 (no adaptation field, payload only)
    // continuity_counter = b????
    // skip = 0x00
    // -- payload follows
    // table_id = 0x02
    // section_syntax_indicator = b1
    // must_be_zero = b0
    // reserved = b11
    // section_length = 0x???
    // program_number = 0x0001
    // reserved = b11
    // version_number = b00001
    // current_next_indicator = b1
    // section_number = 0x00
    // last_section_number = 0x00
    // reserved = b111
    // PCR_PID = b? ???? ???? ???? (13 bits)
    // reserved = b1111
    // program_info_length = 0x000
    //   one or more elementary stream descriptions follow:
    //   stream_type = 0x??
    //   reserved = b111
    //   elementary_PID = b? ???? ???? ???? (13 bits)
    //   reserved = b1111
    //   ES_info_length = 0x000
    // CRC = 0x????????

    static const uint8_t kData[] = {
        0x47,
        0x41, 0xe0, 0x10, 0x00,  // b0100 0001 1110 0000 0001 ???? 0000 0000
        0x02, 0xb0, 0x00, 0x00,  // b0000 0010 1011 ???? ???? ???? 0000 0000
        0x01, 0xc3, 0x00, 0x00,  // b0000 0001 1100 0011 0000 0000 0000 0000
        0xe0, 0x00, 0xf0, 0x00   // b111? ???? ???? ???? 1111 0000 0000 0000
    };

    sp<ABuffer> buffer = new ABuffer(188);
    memset(buffer->data(), 0xff, buffer->size());
    memcpy(buffer->data(), kData, sizeof(kData));

    if (mPMTContinuityCounter == 16) {
        mPMTContinuityCounter = 0;
    }
    buffer->data()[3] |= mPMTContinuityCounter;
   mPMTContinuityCounter++;

    size_t section_length = 5 * mSources.size() + 4 + 9;
    buffer->data()[6] |= section_length >> 8;
    buffer->data()[7] = section_length & 0xff;

    static const unsigned kPCR_PID = 0x1d0;
    buffer->data()[13] |= (kPCR_PID >> 8) & 0x1f;
    buffer->data()[14] = kPCR_PID & 0xff;

    uint8_t *ptr = &buffer->data()[sizeof(kData)];
    for (size_t i = 0; i < mSources.size(); ++i) {
        *ptr++ = mSources.editItemAt(i)->streamType();

        const unsigned ES_PID = 0x1e0 + i + 1;
        *ptr++ = 0xe0 | (ES_PID >> 8);
        *ptr++ = ES_PID & 0xff;
        *ptr++ = 0xf0;
        *ptr++ = 0x00;
    }

    uint32_t crc = htonl(crc32(&buffer->data()[5], 12+mSources.size()*5));
    memcpy(&buffer->data()[17+mSources.size()*5], &crc, sizeof(crc));

    CHECK_EQ(internalWrite(buffer->data(), buffer->size()), buffer->size());
}

void MPEG2TSWriter::writeAccessUnit(
        int32_t sourceIndex, const sp<ABuffer> &accessUnit) {
    // 0x47
    // transport_error_indicator = b0
    // payload_unit_start_indicator = b1
    // transport_priority = b0
    // PID = b0 0001 1110 ???? (13 bits) [0x1e0 + 1 + sourceIndex]
    // transport_scrambling_control = b00
    // adaptation_field_control = b??
    // continuity_counter = b????
    // -- payload follows
    // packet_startcode_prefix = 0x000001
    // stream_id = 0x?? (0xe0 for avc video, 0xc0 for aac audio)
    // PES_packet_length = 0x????
    // reserved = b10
    // PES_scrambling_control = b00
    // PES_priority = b0
    // data_alignment_indicator = b1
    // copyright = b0
    // original_or_copy = b0
    // PTS_DTS_flags = b10  (PTS only)
    // ESCR_flag = b0
    // ES_rate_flag = b0
    // DSM_trick_mode_flag = b0
    // additional_copy_info_flag = b0
    // PES_CRC_flag = b0
    // PES_extension_flag = b0
    // PES_header_data_length = 0x05
    // reserved = b0010 (PTS)
    // PTS[32..30] = b???
    // reserved = b1
    // PTS[29..15] = b??? ???? ???? ???? (15 bits)
    // reserved = b1
    // PTS[14..0] = b??? ???? ???? ???? (15 bits)
    // reserved = b1
    // the first fragment of "buffer" follows

    sp<ABuffer> buffer = new ABuffer(188);
    memset(buffer->data(), 0xff, buffer->size());

    const unsigned PID = 0x1e0 + sourceIndex + 1;

    const unsigned continuity_counter =
        mSources.editItemAt(sourceIndex)->incrementContinuityCounter();

    // XXX if there are multiple streams of a kind (more than 1 audio or
    // more than 1 video) they need distinct stream_ids.
    const unsigned stream_id =
        mSources.editItemAt(sourceIndex)->streamType() == 0x0f ? 0xc0 : 0xe0;

    int64_t timeUs;
    CHECK(accessUnit->meta()->findInt64("timeUs", &timeUs));

    uint32_t PTS = (timeUs * 9ll) / 100ll;

    size_t PES_packet_length = accessUnit->size() + 8;
    bool padding = (accessUnit->size() < (188 - 18));

    if (PES_packet_length >= 65536) {
        // This really should only happen for video.
        CHECK_EQ(stream_id, 0xe0u);

        // It's valid to set this to 0 for video according to the specs.
        PES_packet_length = 0;
    }

    uint8_t *ptr = buffer->data();
    *ptr++ = 0x47;
    *ptr++ = 0x40 | (PID >> 8);
    *ptr++ = PID & 0xff;
    *ptr++ = (padding ? 0x30 : 0x10) | continuity_counter;
    if (padding) {
        int paddingSize = 188 - accessUnit->size() - 18;
        *ptr++ = paddingSize - 1;
        if (paddingSize >= 2) {
            *ptr++ = 0x00;
            ptr += paddingSize - 2;
        }
    }
    *ptr++ = 0x00;
    *ptr++ = 0x00;
    *ptr++ = 0x01;
    *ptr++ = stream_id;
    *ptr++ = PES_packet_length >> 8;
    *ptr++ = PES_packet_length & 0xff;
    *ptr++ = 0x84;
    *ptr++ = 0x80;
    *ptr++ = 0x05;
    *ptr++ = 0x20 | (((PTS >> 30) & 7) << 1) | 1;
    *ptr++ = (PTS >> 22) & 0xff;
    *ptr++ = (((PTS >> 15) & 0x7f) << 1) | 1;
    *ptr++ = (PTS >> 7) & 0xff;
    *ptr++ = ((PTS & 0x7f) << 1) | 1;

    size_t sizeLeft = buffer->data() + buffer->size() - ptr;
    size_t copy = accessUnit->size();
    if (copy > sizeLeft) {
        copy = sizeLeft;
    }

    memcpy(ptr, accessUnit->data(), copy);

    CHECK_EQ(internalWrite(buffer->data(), buffer->size()), buffer->size());

    size_t offset = copy;
    while (offset < accessUnit->size()) {
        bool lastAccessUnit = ((accessUnit->size() - offset) < 184);
        // for subsequent fragments of "buffer":
        // 0x47
        // transport_error_indicator = b0
        // payload_unit_start_indicator = b0
        // transport_priority = b0
        // PID = b0 0001 1110 ???? (13 bits) [0x1e0 + 1 + sourceIndex]
        // transport_scrambling_control = b00
        // adaptation_field_control = b??
        // continuity_counter = b????
        // the fragment of "buffer" follows.

        memset(buffer->data(), 0xff, buffer->size());

        const unsigned continuity_counter =
            mSources.editItemAt(sourceIndex)->incrementContinuityCounter();

        ptr = buffer->data();
        *ptr++ = 0x47;
        *ptr++ = 0x00 | (PID >> 8);
        *ptr++ = PID & 0xff;
        *ptr++ = (lastAccessUnit ? 0x30 : 0x10) | continuity_counter;

        if (lastAccessUnit) {
            // Pad packet using an adaptation field
            // Adaptation header all to 0 execpt size
            uint8_t paddingSize = (uint8_t)184 - (accessUnit->size() - offset);
            *ptr++ = paddingSize - 1;
            if (paddingSize >= 2) {
                *ptr++ = 0x00;
                ptr += paddingSize - 2;
            }
        }

        size_t sizeLeft = buffer->data() + buffer->size() - ptr;
        size_t copy = accessUnit->size() - offset;
        if (copy > sizeLeft) {
            copy = sizeLeft;
        }

        memcpy(ptr, accessUnit->data() + offset, copy);
        CHECK_EQ(internalWrite(buffer->data(), buffer->size()),
                 buffer->size());

        offset += copy;
    }
}

void MPEG2TSWriter::writePCR(){
    ALOGV("writePCR");
    // PCR stream
    // 0x47
    // transport_error_indicator = b0
    // payload_unit_start_indicator = b1
    // transport_priority = b0
    // PID = kPCR_PID (13 bits)
    // transport_scrambling_control = b00
    // adaptation_field_control = b10 (adaptation field only, no payload)
    // continuity_counter = b0000 (does not increment)
    // adaptation_field_length = 183
    // discontinuity_indicator = b0
    // random_access_indicator = b0
    // elementary_stream_priority_indicator = b0
    // PCR_flag = b1
    // OPCR_flag = b0
    // splicing_point_flag = b0
    // transport_private_data_flag = b0
    // adaptation_field_extension_flag = b0
    // program_clock_reference_base = b?????????????????????????????????
    // reserved = b111111
    // program_clock_reference_extension = b?????????

    int64_t nowUs = ALooper::GetNowUs();

    uint64_t PCR = nowUs * 27;  // PCR based on a 27MHz clock
    uint64_t PCR_base = PCR / 300;
    uint32_t PCR_ext = PCR % 300;
    uint32_t kPID_PCR = 0x1d0;

    sp<ABuffer> buffer = new ABuffer(188);
    memset(buffer->data(), 0xff, buffer->size());

    buffer->data()[0] = 0x47 ;
    buffer->data()[1] = 0x40 | (kPID_PCR >> 8);
    buffer->data()[2] = kPID_PCR & 0xff;
    buffer->data()[3] = 0x20;
    buffer->data()[4] = 0xb7;   // adaptation_field_length
    buffer->data()[5] = 0x10;
    buffer->data()[6] = (PCR_base >> 25) & 0xff;
    buffer->data()[7] = (PCR_base >> 17) & 0xff;
    buffer->data()[8] = (PCR_base >> 9) & 0xff;
    buffer->data()[9] = ((PCR_base & 1) << 7) | 0x7e | ((PCR_ext >> 8) & 1);
    buffer->data()[10] = (PCR_ext & 0xff);

    uint32_t crc = htonl(crc32(&buffer->data()[5], 12));
    memcpy(&buffer->data()[17+mSources.size()], &crc, sizeof(crc));

    CHECK_EQ(internalWrite(buffer->data(), buffer->size()), buffer->size());
}

void MPEG2TSWriter::writeTS() {

    ++mNumTSPacketsWritten;
    ALOGV("writeTS mNumTSPacketsWritten mNumTSPacketsBeforeMeta %lld %lld",mNumTSPacketsWritten,mNumTSPacketsBeforeMeta);
    if (mNumTSPacketsWritten >= mNumTSPacketsBeforeMeta) {
        writeProgramAssociationTable();
        writeProgramMap();

        writePCR();
        //mNumTSPacketsBeforeMeta = mNumTSPacketsWritten + 2500;
        mNumTSPacketsBeforeMeta = mNumTSPacketsWritten + 100;
    }
}

void MPEG2TSWriter::initCrcTable() {
    uint32_t poly = 0x04C11DB7;

    for (int i = 0; i < 256; i++) {
        uint32_t crc = i << 24;
        for (int j = 0; j < 8; j++) {
            crc = (crc << 1) ^ ((crc & 0x80000000) ? (poly) : 0);
        }
        mCrcTable[i] = crc;
    }
}

/**
 * Compute CRC32 checksum for buffer starting at offset start and for length
 * bytes.
 */
uint32_t MPEG2TSWriter::crc32(const uint8_t *p_start, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t *p;

    for (p = p_start; p < p_start + length; p++) {
        crc = (crc << 8) ^ mCrcTable[((crc >> 24) ^ *p) & 0xFF];
    }

    return crc;
}

inline void MPEG2TSWriter::writeAwCache(const void *write_data, size_t size) {
   size_t nLeftSize = 0;
   size_t needWriteSize = size;

   if((mAwCache.mValidCacheSize + needWriteSize) <= mAwCache.mCacheSize) {
       memcpy(mAwCache.mCacheData + mAwCache.mValidCacheSize, (uint8_t *)write_data, needWriteSize);
       mAwCache.mValidCacheSize += needWriteSize;
       if(mAwCache.mValidCacheSize == mAwCache.mCacheSize)
       {
           fwrite(mAwCache.mCacheData, 1, mAwCache.mCacheSize, mFile);
           mAwCache.mValidCacheSize = 0;
       }
   }
   else
   {
       size_t nSize0 = mAwCache.mCacheSize - mAwCache.mValidCacheSize;
       nLeftSize = needWriteSize - nSize0;
       memcpy(mAwCache.mCacheData + mAwCache.mValidCacheSize, (uint8_t *)write_data, nSize0);

       fwrite(mAwCache.mCacheData, 1, mAwCache.mCacheSize, mFile);
       mAwCache.mValidCacheSize = 0;

       size_t write_num = nLeftSize/mAwCache.mCacheSize*mAwCache.mCacheSize;
       if(write_num > 0)
       {
           fwrite((uint8_t *)write_data + nSize0, 1, write_num, mFile);
           nLeftSize -= write_num;
       }

       if(nLeftSize > 0)
       {
           memcpy(mAwCache.mCacheData, (uint8_t *)write_data + nSize0 + write_num, nLeftSize);
           mAwCache.mValidCacheSize = nLeftSize;
       }
   }
}

ssize_t MPEG2TSWriter::internalWrite(const void *data, size_t size) {
    if (mFile != NULL) {
       if (mAwCache.mNeedCache) {
           writeAwCache(data, size);
           return size;
       }
       else
           return fwrite(data, 1, size, mFile);
    }

    return (*mWriteFunc)(mWriteCookie, data, size);
}

}  // namespace android

