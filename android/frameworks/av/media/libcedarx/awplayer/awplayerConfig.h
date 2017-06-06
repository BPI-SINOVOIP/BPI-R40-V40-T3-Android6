/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : awplayerConfig.h
 * Description : playerConfig
 * History :
 *
 */


#ifndef AWPLAYER_CONFIG_H
#define AWPLAYER_CONFIG_H

#define AWPLAYER_CONFIG_DISABLE_VIDEO       0
#define AWPLAYER_CONFIG_DISABLE_AUDIO       0
#define AWPLAYER_CONFIG_DISABLE_SUBTITLE    0
#define AWPLAYER_CONFIG_DISALBE_MULTI_AUDIO    0

#define WIDTH_4K        3840
#define HEIGHT_4K        2160
#define WIDTH_1080P        1920
#define HEIGHT_1080P    1080

#ifdef FILE_AWPLAYER_CPP
typedef struct CACHE_PARAM_CONFIGURATION
{
    const char* strApkName;

    /* 0:CACHE_POLICY_ADAPTIVE, 1:CACHE_POLICY_QUICK,
     * 2:CACHE_POLICY_SMOOTH,   3:CACHE_POLICY_USER_SPECIFIC_PARAMS.
     * generally, adaptive mode is suitable for vod,
     * quick mode is best for live stream or with a high speed network,
     * smooth mode is best for playing high bitrate video at a speed limited network,
     * such as DLNA playing video recoded by cellphone(bitrate at 20MBps).
     */
    int         eCachePolicy;
    int         nStartPlaySize;
    int         nStartPlayTimeMs;
    int         nCacheBufferSize;
}CacheParamConfig;

static CacheParamConfig CacheParamForSpecificApk[] =
{
    {"com.togic.livevideo", 3, 0, 2000, 20*1024*1024},
    {"com.ysten.launcher2d", CACHE_POLICY_QUICK, 512*1024, 2000, 20*1024*1024},
    {"com.galaxyitv.video", CACHE_POLICY_QUICK, 0, 1000, 20*1024*1024},
    {NULL, 1, 0, 0, 0}
};
#endif

#endif

