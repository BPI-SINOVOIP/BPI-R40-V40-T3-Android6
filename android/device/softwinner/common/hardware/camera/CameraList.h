#ifndef __CAMERA_LIST_H__
#define __CAMERA_LIST_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define CAMERA_LIST_KEY_CONFIG_PATH "/system/etc/cameralist.cfg"

#define KEY_LIST_LENGTH 8192

#define kCAMERA_LIST                    "key_camera_list"
//#define kCAMERA_EXIF_MODEL                    "key_camera_exif_model"

#ifndef DBG_ENABLE
#define DBG_ENABLE 1
#endif

#if  DBG_ENABLE
     #define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
     #define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)
     #define LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
     #define LOGW(...)  __android_log_print(ANDROID_LOG_WARN,LOG_TAG,__VA_ARGS__)
     #define LOGF(...)  __android_log_print(ANDROID_LOG_FATAL,LOG_TAG,__VA_ARGS__)
     #define LOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE,LOG_TAG,__VA_ARGS__)
#else
     #define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
     #define LOGD(...)
     #define LOGI(...)
     #define LOGW(...)
     #define LOGF(...)
     #define LOGV(...)
#endif

class CameraList
{
public:
    CameraList();
    ~CameraList();
    char mCameraDeviceList[KEY_LIST_LENGTH];

private:
    bool readKey(char *key, char *value);
    void getValue(char *line, char *value);
    bool usedKey(char *value);

    FILE * mhKeyFile;

};

#endif // __CAMERA_LIST_H__
