#ifndef __LIBVE_DECORDER_H__
#define __LIBVE_DECORDER_H__
/*
#include "libve/libve.h"
#include "libve/libve_adapter.h"
#include "libve/libve_typedef.h"
#include "libve/libcedarv.h"
*/
#include "libve_typedef.h"
#include "cedarx_hardware.h"
#include "libcedarv.h"
#include "drv_display.h"
//#include <libve/Drv_display.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <pthread.h>
#include "CDX_Resource_Manager.h"

//#include "CameraDebug.h"

#define DBG_ENABLE 0

#define VE_MUTEX_ENABLE 1

#if  VE_MUTEX_ENABLE
#define decorder_mutex_lock(x) ve_mutex_lock(x)
#define decorder_mutex_unlock(x) ve_mutex_unlock(x)
#define decorder_mutex_init(x,y) ve_mutex_init(x,y)
#define decorder_mutex_destroy(x) ve_mutex_destroy(x)

#else
#define decorder_mutex_lock(x)
#define decorder_mutex_unlock(x)
#define decorder_mutex_init(x,y)
#define decorder_mutex_destroy(x)

#endif

#ifdef __cplusplus
extern "C" {
#endif

enum FORMAT_CONVERT_COLORFORMAT {
    CONVERT_COLOR_FORMAT_NONE = 0,
    CONVERT_COLOR_FORMAT_YUV420PLANNER,
    CONVERT_COLOR_FORMAT_YVU420PLANNER,
    CONVERT_COLOR_FORMAT_YUV420MB,
    CONVERT_COLOR_FORMAT_YUV422MB,
};

typedef struct ScalerParameter{
    int mode; //0: YV12 1:thumb yuv420p
    int format_in;
    int format_out;

    int width_in;
    int height_in;

    int width_out;
    int height_out;

    void *addr_y_in;
    void *addr_c_in;
    unsigned int addr_y_out;
    unsigned int addr_u_out;
    unsigned int addr_v_out;
}ScalerParameter;

void Libve_dec(cedarv_decoder_t** decoder,const void *in,void *out,void *decorder_stream_info,void *decorder_data_info,char* outfmt,ve_mutex_t *decorder_mutex);
int Libve_init(cedarv_decoder_t** decoder,cedarv_stream_info_t*stream_info,int scale,ve_mutex_t *decorder_mutex);
int Libve_exit(cedarv_decoder_t** decoder,ve_mutex_t *decorder_mutex);
//int HardwarePictureScaler(ScalerParameter *cdx_scaler_para,__disp_pixel_fmt_t informat);

#ifdef __cplusplus
}
#endif


#endif  /* __LIBVE_DECORDER_H__ */

