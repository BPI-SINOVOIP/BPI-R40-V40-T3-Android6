#include "Libve_Decoder.h"
//#include <CDX_Debug.h>
#define LOG_TAG    "Libev_decorder"
#include<android/log.h>
#include <stdio.h>
#include <time.h>

#define USE_ION_MEM_ALLOCATOR

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

#ifdef USE_ION_MEM_ALLOCATOR
extern int ion_alloc_open();
extern int ion_alloc_close();
extern int ion_alloc_alloc(int size);
extern void ion_alloc_free(void * pbuf);
extern int ion_alloc_vir2phy(void * pbuf);
extern int ion_alloc_phy2vir(void * pbuf);
extern void ion_flush_cache(void* startAddr, int size);
extern void ion_flush_cache_all();


#else USE_SUNXI_MEM_ALLOCATOR
extern  int sunxi_alloc_open();
extern int sunxi_alloc_close();
extern int sunxi_alloc_alloc(int size);
extern  void sunxi_alloc_free(void * pbuf);
extern  int sunxi_alloc_vir2phy(void * pbuf);
extern  int sunxi_alloc_phy2vir(void * pbuf);
extern  void sunxi_flush_cache(void* startAddr, int size);
extern  void sunxi_flush_cache_all();

#endif


static long long GetNowUs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (long long)tv.tv_sec * 1000000ll + tv.tv_usec;
}

static void savefile(char *addr,void *p,int length)
{
    int fd;
    fd = open(addr,O_CREAT|O_RDWR|O_TRUNC,0);
    if(!fd) {
        LOGD("Open file error");
        //return false;
    }
    if(write(fd,p,length))  {
        LOGD("write file successfully");
        close(fd);
        //return true;
    }
    else {
        LOGD("write file fail");
        close(fd);
        //return false;
    }

}
/*
//convert MB to yv12  by DE
int HardwarePictureScaler(ScalerParameter *cdx_scaler_para,__disp_pixel_fmt_t informat)
{
    unsigned long arg[4] = {0,0,0,0};
    int scaler_hdl;
    __disp_scaler_para_t scaler_para;
    int dispfh;

    dispfh = open("/dev/disp", O_RDWR);
    if(dispfh == -1){
        LOGE("Open /dev/disp fail");
        return -1;
    }

    scaler_hdl = ioctl(dispfh, DISP_CMD_SCALER_REQUEST, (unsigned long) arg);
    if(scaler_hdl == -1){
        LOGE("request scaler fail");
        return -1;
    }

    memset(&scaler_para, 0, sizeof(__disp_scaler_para_t));
    scaler_para.input_fb.addr[0] = (unsigned int)cdx_scaler_para->addr_y_in;//
    scaler_para.input_fb.addr[1] = (unsigned int)cdx_scaler_para->addr_c_in;//
    scaler_para.input_fb.size.width = cdx_scaler_para->width_in;//
    scaler_para.input_fb.size.height = cdx_scaler_para->height_in;//
    scaler_para.input_fb.format =  informat;
    scaler_para.input_fb.seq = DISP_SEQ_UVUV;
    scaler_para.input_fb.mode = DISP_MOD_MB_UV_COMBINED;
    scaler_para.input_fb.br_swap = 0;
    scaler_para.input_fb.cs_mode = DISP_BT601;
    scaler_para.source_regn.x = 0;
    scaler_para.source_regn.y = 0;
    scaler_para.source_regn.width = cdx_scaler_para->width_in;//
    scaler_para.source_regn.height = cdx_scaler_para->height_in;//
    scaler_para.output_fb.addr[0] = cdx_scaler_para->addr_y_out;//
    scaler_para.output_fb.addr[1] = cdx_scaler_para->addr_u_out;//
    scaler_para.output_fb.addr[2] = cdx_scaler_para->addr_v_out;//
    scaler_para.output_fb.size.width = cdx_scaler_para->width_out;//
    scaler_para.output_fb.size.height = cdx_scaler_para->height_out;//
    scaler_para.output_fb.format = DISP_FORMAT_YUV420;
    scaler_para.output_fb.seq = DISP_SEQ_P3210;
    scaler_para.output_fb.mode = DISP_MOD_NON_MB_PLANAR;
    scaler_para.output_fb.br_swap = 0;
    scaler_para.output_fb.cs_mode = DISP_BT601;

    LOGV( "scaler parameter check: w:%d h:%d y_in:0x%x c_in:0x%x out_0:0x%x out_1:0x%x out_2:0x%x format:0x%x",
            cdx_scaler_para->width_in,cdx_scaler_para->height_in,cdx_scaler_para->addr_y_in,cdx_scaler_para->addr_c_in,
            scaler_para.output_fb.addr[0],scaler_para.output_fb.addr[1],scaler_para.output_fb.addr[2],scaler_para.output_fb.format);

    arg[1] = scaler_hdl;
    arg[2] = (unsigned long) &scaler_para;
    ioctl(dispfh, DISP_CMD_SCALER_EXECUTE, (unsigned long) arg);

    arg[1] = scaler_hdl;
    ioctl(dispfh, DISP_CMD_SCALER_RELEASE, (unsigned long) arg);

    close(dispfh);

    return 0;
}
*/

static void YUYVToNV12(const void* y,const void *u, void *nv12, int width, int height)
{
    int i,j;
    int phy;
    u8* Y   = (u8*)nv12;
    u8* UV  = (u8*)Y + width * height;
    u8* yuyv    =(u8*)malloc(2*width * height*sizeof(u8));

    memset(yuyv,0,2*width * height);
    memcpy(nv12,y,width * height);
    memcpy(yuyv+width * height,u,width * height);

    for(i = 0; i < height; i += 2)
    {
        for (j = 0; j < width; j++)
        {
            *(uint8_t*)((uint8_t*)Y + i * width + j) = *(uint8_t*)((uint8_t*)yuyv + i * width * 2 + j * 2);
            *(uint8_t*)((uint8_t*)Y + (i + 1) * width + j) = *(uint8_t*)((uint8_t*)yuyv + (i + 1) * width * 2 + j * 2);
            *(uint8_t*)((uint8_t*)UV + ((i * width) >> 1) + j) = *(uint8_t*)((uint8_t*)yuyv + i * width * 2 + j * 2 + 1);
        }
    }
    free(yuyv);
}

//convert MB32*32  to  yuv
static void map32x32_to_yuv(unsigned char* srcY, unsigned char* tarY,unsigned int coded_width,unsigned int coded_height)
{
    unsigned int i,j,l,m,n;
    unsigned int mb_width,mb_height,twomb_line, twomb_width, recon_width;
    unsigned long offset;
    unsigned char *ptr;
    unsigned char *dst_asm,*src_asm;
    unsigned vdecbuf_width, vdecbuf_height;
    int nWidthMatchFlag;
    int nLeftValidLine;  //in the bottom macroblock(32*32), the valid line is < 32.
    ptr = srcY;
//  mb_width = (coded_width+15)>>4;
//  mb_height = (coded_height+15)>>4;
//  twomb_line = (mb_height+1)>>1;
//  recon_width = (mb_width+1)&0xfffffffe;

    mb_width = ((coded_width+31)&~31) >>4;
    mb_height = ((coded_height+31)&~31) >>4;
    twomb_line = (mb_height+1)>>1;
    recon_width = (mb_width+1)&0xfffffffe;
    twomb_width = (mb_width+1)>>1;
    if(twomb_line < 1 || twomb_width < 1)
    {
        LOGE("fatal error! twomb_line=%d, twomb_width=%d", twomb_line, twomb_width);
    }
    vdecbuf_width = twomb_width*32;
    vdecbuf_height = twomb_line*32;
    if(vdecbuf_width > coded_width)
    {
        nWidthMatchFlag = 0;
        if((vdecbuf_width - coded_width) != 16)
        {
            LOGW("(f:%s, l:%d) fatal error! vdecbuf_width=%d, gpubuf_width=%d,  the program will crash!", __FUNCTION__, __LINE__, vdecbuf_width, coded_width);
        }
        else
        {
            //LOGV("(f:%s, l:%d) Be careful! vdecbuf_width=%d, gpubuf_width=%d", __FUNCTION__, __LINE__, vdecbuf_width, coded_width);
        }
    }
    else if(vdecbuf_width == coded_width)
    {
        nWidthMatchFlag = 1;
    }
    else
    {
        LOGW("(f:%s, l:%d) fatal error! vdecbuf_width=%d <= gpubuf_width=%d, the program will crash!", __FUNCTION__, __LINE__, vdecbuf_width, coded_width);
        nWidthMatchFlag = 0;
    }
    for(i=0;i<twomb_line-1;i++)   //twomb line number
    {
        for(j=0;j<twomb_width-1;j++)   //macroblock(32*32) number in one line
        {
            for(l=0;l<32;l++)
            {
                //first mb
                m=i*32 + l;     //line num
                n= j*32;        //byte num in one line
                offset = m*coded_width + n;
                //memcpy(tarY+offset,ptr,32);
                dst_asm = tarY+offset;
                src_asm = ptr;
                asm volatile (
                        "vld1.8         {d0 - d3}, [%[src_asm]]              \n\t"
                        "vst1.8         {d0 - d3}, [%[dst_asm]]              \n\t"
                        : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                        :  //[srcY] "r" (srcY)
                        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                        );

                ptr += 32;  //32 byte in one process.
            }
        }
        //process last macroblock of one line, gpu buf must be 16byte align or 32 byte align
        { //last mb of one line
            for(l=0;l<32;l++)
            {
                //first mb
                m=i*32 + l;     //line num
                n= j*32;        //byte num in one line
                offset = m*coded_width + n;
                //memcpy(tarY+offset,ptr,32);
                dst_asm = tarY+offset;
                src_asm = ptr;
                if(nWidthMatchFlag)
                {
                    asm volatile (
                            "vld1.8         {d0 - d3}, [%[src_asm]]              \n\t"
                            "vst1.8         {d0 - d3}, [%[dst_asm]]              \n\t"
                            : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                            :  //[srcY] "r" (srcY)
                            : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                            );
                }
                else
                {
                    asm volatile (
                            "vld1.8         {d0,d1}, [%[src_asm]]              \n\t"
                            "vst1.8         {d0,d1}, [%[dst_asm]]              \n\t"
                            : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                            :  //[srcY] "r" (srcY)
                            : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                            );
                }

                ptr += 32;  //32 byte in one process.
            }
        }
    }
    //last twomb line, we process it alone
    nLeftValidLine = coded_height - (twomb_line-1)*32;
    if(nLeftValidLine!=32)
    {
        //LOGV("(f:%s, l:%d)hehehaha,gpuBufHeight[%d] is not 32 align", __FUNCTION__, __LINE__, nLeftValidLine);
    }
    for(j=0;j<twomb_width-1;j++)   //macroblock(32*32) number in one line
    {
        for(l=0;l<nLeftValidLine;l++)
        {
            //first mb
            m=i*32 + l;     //line num
            n= j*32;        //byte num in one line
            offset = m*coded_width + n;
            //memcpy(tarY+offset,ptr,32);
            dst_asm = tarY+offset;
            src_asm = ptr;
            asm volatile (
                    "vld1.8         {d0 - d3}, [%[src_asm]]              \n\t"
                    "vst1.8         {d0 - d3}, [%[dst_asm]]              \n\t"
                    : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                    :  //[srcY] "r" (srcY)
                    : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                    );

            ptr += 32;  //32 byte in one process.
        }
        ptr += (32-nLeftValidLine)*32;
    }
    //process last macroblock of last line, gpu buf must be 16byte align or 32 byte align
    { //last mb of last line
        for(l=0;l<nLeftValidLine;l++)
        {
            //first mb
            m=i*32 + l;     //line num
            n= j*32;        //byte num in one line
            offset = m*coded_width + n;
            //memcpy(tarY+offset,ptr,32);
            dst_asm = tarY+offset;
            src_asm = ptr;
            if(nWidthMatchFlag)
            {
                asm volatile (
                        "vld1.8         {d0 - d3}, [%[src_asm]]              \n\t"
                        "vst1.8         {d0 - d3}, [%[dst_asm]]              \n\t"
                        : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                        :  //[srcY] "r" (srcY)
                        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                        );
            }
            else
            {
                asm volatile (
                        "vld1.8         {d0,d1}, [%[src_asm]]              \n\t"
                        "vst1.8         {d0,d1}, [%[dst_asm]]              \n\t"
                        : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                        :  //[srcY] "r" (srcY)
                        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                        );
            }

            ptr += 32;  //32 byte in one process.
        }
        ptr += (32-nLeftValidLine)*32;
    }
}

static void map32x32_to_yuv_nv21(unsigned char* srcY, unsigned char* tarY,unsigned int coded_width,unsigned int coded_height)
{
    unsigned int i,j,l,m,n;
    unsigned int mb_width,mb_height,twomb_line, twomb_width, recon_width;
    unsigned long offset;
    unsigned char *ptr;
    unsigned char *dst_asm,*src_asm;
    unsigned vdecbuf_width, vdecbuf_height;
    int nWidthMatchFlag;
    int nLeftValidLine;  //in the bottom macroblock(32*32), the valid line is < 32.
    ptr = srcY;
//  mb_width = (coded_width+15)>>4;
//  mb_height = (coded_height+15)>>4;
//  twomb_line = (mb_height+1)>>1;
//  recon_width = (mb_width+1)&0xfffffffe;

    mb_width = ((coded_width+31)&~31) >>4;
    mb_height = ((coded_height+31)&~31) >>4;
    twomb_line = (mb_height+1)>>1;
    recon_width = (mb_width+1)&0xfffffffe;
    twomb_width = (mb_width+1)>>1;
    if(twomb_line < 1 || twomb_width < 1)
    {
        LOGE("fatal error! twomb_line=%d, twomb_width=%d", twomb_line, twomb_width);
    }
    vdecbuf_width = twomb_width*32;
    vdecbuf_height = twomb_line*32;
    if(vdecbuf_width > coded_width)
    {
        nWidthMatchFlag = 0;
        if((vdecbuf_width - coded_width) != 16)
        {
            LOGW("(f:%s, l:%d) fatal error! vdecbuf_width=%d, gpubuf_width=%d,  the program will crash!", __FUNCTION__, __LINE__, vdecbuf_width, coded_width);
        }
        else
        {
            //LOGV("(f:%s, l:%d) Be careful! vdecbuf_width=%d, gpubuf_width=%d", __FUNCTION__, __LINE__, vdecbuf_width, coded_width);
        }
    }
    else if(vdecbuf_width == coded_width)
    {
        nWidthMatchFlag = 1;
    }
    else
    {
        LOGW("(f:%s, l:%d) fatal error! vdecbuf_width=%d <= gpubuf_width=%d, the program will crash!", __FUNCTION__, __LINE__, vdecbuf_width, coded_width);
        nWidthMatchFlag = 0;
    }
    for(i=0;i<twomb_line-1;i++)   //twomb line number
    {
        for(j=0;j<twomb_width-1;j++)   //macroblock(32*32) number in one line
        {
            for(l=0;l<32;l++)
            {
                //first mb
                m=i*32 + l;     //line num
                n= j*32;        //byte num in one line
                offset = m*coded_width + n;
                //memcpy(tarY+offset,ptr,32);
                dst_asm = tarY+offset;
                src_asm = ptr;
                asm volatile (
/*                      "vld1.8         {d0 - d3}, [%[src_asm]]              \n\t"
                        "vst1.8         {d0 - d3}, [%[dst_asm]]              \n\t"
*/
                    "vld2.8         {d0 - d1}, [%[src_asm]]!              \n\t"
                    "vld2.8         {d2 - d3}, [%[src_asm]]               \n\t"
                    "vswp           d0, d1                                \n\t"
                    "vswp           d2, d3                                \n\t"
                    "vst2.8         {d0 - d1}, [%[dst_asm]]!              \n\t"
                    "vst2.8         {d2 - d3}, [%[dst_asm]]               \n\t"

                        : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                        :  //[srcY] "r" (srcY)
                        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                        );

                ptr += 32;  //32 byte in one process.
            }
        }
        //process last macroblock of one line, gpu buf must be 16byte align or 32 byte align
        { //last mb of one line
            for(l=0;l<32;l++)
            {
                //first mb
                m=i*32 + l;     //line num
                n= j*32;        //byte num in one line
                offset = m*coded_width + n;
                //memcpy(tarY+offset,ptr,32);
                dst_asm = tarY+offset;
                src_asm = ptr;
                if(nWidthMatchFlag)
                {
                    asm volatile (
//                          "vld1.8         {d0 - d3}, [%[src_asm]]              \n\t"
//                          "vst1.8         {d0 - d3}, [%[dst_asm]]              \n\t"
                    "vld2.8         {d0 - d1}, [%[src_asm]]!              \n\t"
                    "vld2.8         {d2 - d3}, [%[src_asm]]               \n\t"
                    "vswp           d0, d1                                \n\t"
                    "vswp           d2, d3                                \n\t"
                    "vst2.8         {d0 - d1}, [%[dst_asm]]!              \n\t"
                    "vst2.8         {d2 - d3}, [%[dst_asm]]               \n\t"

                            : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                            :  //[srcY] "r" (srcY)
                            : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                            );
                }
                else
                {
                    asm volatile (
                            "vld1.8         {d0,d1}, [%[src_asm]]              \n\t"
                            "vst1.8         {d0,d1}, [%[dst_asm]]              \n\t"
                            : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                            :  //[srcY] "r" (srcY)
                            : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                            );
                }

                ptr += 32;  //32 byte in one process.
            }
        }
    }
    //last twomb line, we process it alone
    nLeftValidLine = coded_height - (twomb_line-1)*32;
    if(nLeftValidLine!=32)
    {
        //LOGV("(f:%s, l:%d)hehehaha,gpuBufHeight[%d] is not 32 align", __FUNCTION__, __LINE__, nLeftValidLine);
    }
    for(j=0;j<twomb_width-1;j++)   //macroblock(32*32) number in one line
    {
        for(l=0;l<nLeftValidLine;l++)
        {
            //first mb
            m=i*32 + l;     //line num
            n= j*32;        //byte num in one line
            offset = m*coded_width + n;
            //memcpy(tarY+offset,ptr,32);
            dst_asm = tarY+offset;
            src_asm = ptr;
            asm volatile (
//                  "vld1.8         {d0 - d3}, [%[src_asm]]              \n\t"
//                  "vst1.8         {d0 - d3}, [%[dst_asm]]              \n\t"
            "vld2.8         {d0 - d1}, [%[src_asm]]!              \n\t"
            "vld2.8         {d2 - d3}, [%[src_asm]]               \n\t"
            "vswp           d0, d1                                \n\t"
            "vswp           d2, d3                                \n\t"
            "vst2.8         {d0 - d1}, [%[dst_asm]]!              \n\t"
            "vst2.8         {d2 - d3}, [%[dst_asm]]               \n\t"

                    : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                    :  //[srcY] "r" (srcY)
                    : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                    );

            ptr += 32;  //32 byte in one process.
        }
        ptr += (32-nLeftValidLine)*32;
    }
    //process last macroblock of last line, gpu buf must be 16byte align or 32 byte align
    { //last mb of last line
        for(l=0;l<nLeftValidLine;l++)
        {
            //first mb
            m=i*32 + l;     //line num
            n= j*32;        //byte num in one line
            offset = m*coded_width + n;
            //memcpy(tarY+offset,ptr,32);
            dst_asm = tarY+offset;
            src_asm = ptr;
            if(nWidthMatchFlag)
            {
                asm volatile (
//                      "vld1.8         {d0 - d3}, [%[src_asm]]              \n\t"
//                      "vst1.8         {d0 - d3}, [%[dst_asm]]              \n\t"
                "vld2.8         {d0 - d1}, [%[src_asm]]!              \n\t"
                "vld2.8         {d2 - d3}, [%[src_asm]]               \n\t"
                "vswp           d0, d1                                \n\t"
                "vswp           d2, d3                                \n\t"
                "vst2.8         {d0 - d1}, [%[dst_asm]]!              \n\t"
                "vst2.8         {d2 - d3}, [%[dst_asm]]               \n\t"

                        : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                        :  //[srcY] "r" (srcY)
                        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                        );
            }
            else
            {
                asm volatile (
                        "vld1.8         {d0,d1}, [%[src_asm]]              \n\t"
                        "vst1.8         {d0,d1}, [%[dst_asm]]              \n\t"
                        : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                        :  //[srcY] "r" (srcY)
                        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                        );
            }

            ptr += 32;  //32 byte in one process.
        }
        ptr += (32-nLeftValidLine)*32;
    }
}

static void map32x32_to_yuv_skip_oneline(unsigned char* srcY, unsigned char* tarY,unsigned int coded_width,unsigned int coded_height)
{
    unsigned int i,j,l,m,n;
    unsigned int mb_width,mb_height,twomb_line, twomb_width, recon_width;
    unsigned long offset;
    unsigned char *ptr;
    unsigned char *dst_asm,*src_asm;
    unsigned vdecbuf_width, vdecbuf_height;
    int nWidthMatchFlag;
    int nLeftValidLine;  //in the bottom macroblock(32*32), the valid line is < 32.
    ptr = srcY;
//  mb_width = (coded_width+15)>>4;
//  mb_height = (coded_height+15)>>4;
//  twomb_line = (mb_height+1)>>1;
//  recon_width = (mb_width+1)&0xfffffffe;

    mb_width = ((coded_width+31)&~31) >>4;
    mb_height = ((coded_height+31)&~31) >>4;
    twomb_line = (mb_height+1)>>1;
    recon_width = (mb_width+1)&0xfffffffe;
    twomb_width = (mb_width+1)>>1;
    if(twomb_line < 1 || twomb_width < 1)
    {
        LOGE("fatal error! twomb_line=%d, twomb_width=%d", twomb_line, twomb_width);
    }
    vdecbuf_width = twomb_width*32;
    vdecbuf_height = twomb_line*32;
    if(vdecbuf_width > coded_width)
    {
        nWidthMatchFlag = 0;
        if((vdecbuf_width - coded_width) != 16)
        {
            LOGW("(f:%s, l:%d) fatal error! vdecbuf_width=%d, gpubuf_width=%d,  the program will crash!", __FUNCTION__, __LINE__, vdecbuf_width, coded_width);
        }
        else
        {
            //LOGV("(f:%s, l:%d) Be careful! vdecbuf_width=%d, gpubuf_width=%d", __FUNCTION__, __LINE__, vdecbuf_width, coded_width);
        }
    }
    else if(vdecbuf_width == coded_width)
    {
        nWidthMatchFlag = 1;
    }
    else
    {
        LOGW("(f:%s, l:%d) fatal error! vdecbuf_width=%d <= gpubuf_width=%d, the program will crash!", __FUNCTION__, __LINE__, vdecbuf_width, coded_width);
        nWidthMatchFlag = 0;
    }
    for(i=0;i<twomb_line-1;i++)   //twomb line number
    {
        for(j=0;j<twomb_width-1;j++)   //macroblock(32*32) number in one line
        {
            for(l=0;l<16;l++)
            {
                //first mb
                m=i*16 + l;     //line num,skip oneline
                n= j*32;        //byte num in one line
                offset = m*coded_width + n;
                //memcpy(tarY+offset,ptr,32);
                dst_asm = tarY+offset;
                src_asm = ptr;
                asm volatile (
                        "vld1.8         {d0 - d3}, [%[src_asm]]              \n\t"
                        "vst1.8         {d0 - d3}, [%[dst_asm]]              \n\t"
                        : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                        :  //[srcY] "r" (srcY)
                        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                        );

                ptr += 32;  //32 byte in one process.
                ptr += 32;  //32 byte in one process.
            }
        }
        //process last macroblock of one line, gpu buf must be 16byte align or 32 byte align
        { //last mb of one line
            for(l=0;l<16;l++)
            {
                //first mb
                m=i*16 + l;     //line num
                n= j*32;        //byte num in one line
                offset = m*coded_width + n;
                //memcpy(tarY+offset,ptr,32);
                dst_asm = tarY+offset;
                src_asm = ptr;
                if(nWidthMatchFlag)
                {
                    asm volatile (
                            "vld1.8         {d0 - d3}, [%[src_asm]]              \n\t"
                            "vst1.8         {d0 - d3}, [%[dst_asm]]              \n\t"
                            : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                            :  //[srcY] "r" (srcY)
                            : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                            );
                }
                else
                {
                    asm volatile (
                            "vld1.8         {d0,d1}, [%[src_asm]]              \n\t"
                            "vst1.8         {d0,d1}, [%[dst_asm]]              \n\t"
                            : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                            :  //[srcY] "r" (srcY)
                            : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                            );
                }

                ptr += 32;  //32 byte in one process.
                ptr += 32;  //32 byte in one process.
            }
        }
    }
    //last twomb line, we process it alone
    nLeftValidLine = coded_height - (twomb_line-1)*32;
    if(nLeftValidLine!=32)
    {
        //LOGV("(f:%s, l:%d)hehehaha,gpuBufHeight[%d] is not 32 align", __FUNCTION__, __LINE__, nLeftValidLine);
    }
    for(j=0;j<twomb_width-1;j++)   //macroblock(32*32) number in one line
    {
        for(l=0;l<nLeftValidLine/2;l++)
        {
            //first mb
            m=i*16 + l;     //line num
            n= j*32;        //byte num in one line
            offset = m*coded_width + n;
            //memcpy(tarY+offset,ptr,32);
            dst_asm = tarY+offset;
            src_asm = ptr;
            asm volatile (
                    "vld1.8         {d0 - d3}, [%[src_asm]]              \n\t"
                    "vst1.8         {d0 - d3}, [%[dst_asm]]              \n\t"
                    : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                    :  //[srcY] "r" (srcY)
                    : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                    );

            ptr += 32;  //32 byte in one process.
            ptr += 32;  //32 byte in one process.
        }
        ptr += (32-nLeftValidLine)*32;
    }
    //process last macroblock of last line, gpu buf must be 16byte align or 32 byte align
    { //last mb of last line
        for(l=0;l<nLeftValidLine/2;l++)
        {
            //first mb
            m=i*16 + l;     //line num
            n= j*32;        //byte num in one line
            offset = m*coded_width + n;
            //memcpy(tarY+offset,ptr,32);
            dst_asm = tarY+offset;
            src_asm = ptr;
            if(nWidthMatchFlag)
            {
                asm volatile (
                        "vld1.8         {d0 - d3}, [%[src_asm]]              \n\t"
                        "vst1.8         {d0 - d3}, [%[dst_asm]]              \n\t"
                        : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                        :  //[srcY] "r" (srcY)
                        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                        );
            }
            else
            {
                asm volatile (
                        "vld1.8         {d0,d1}, [%[src_asm]]              \n\t"
                        "vst1.8         {d0,d1}, [%[dst_asm]]              \n\t"
                        : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                        :  //[srcY] "r" (srcY)
                        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                        );
            }

            ptr += 32;  //32 byte in one process.
            ptr += 32;  //32 byte in one process.
        }
        ptr += (32-nLeftValidLine)*32;
    }
}

static void map32x32_to_yuv_skip_oneline_nv21(unsigned char* srcY, unsigned char* tarY,unsigned int coded_width,unsigned int coded_height)
{
    unsigned int i,j,l,m,n;
    unsigned int mb_width,mb_height,twomb_line, twomb_width, recon_width;
    unsigned long offset;
    unsigned char *ptr;
    unsigned char *dst_asm,*src_asm;
    unsigned vdecbuf_width, vdecbuf_height;
    int nWidthMatchFlag;
    int nLeftValidLine;  //in the bottom macroblock(32*32), the valid line is < 32.
    ptr = srcY;
//  mb_width = (coded_width+15)>>4;
//  mb_height = (coded_height+15)>>4;
//  twomb_line = (mb_height+1)>>1;
//  recon_width = (mb_width+1)&0xfffffffe;

    mb_width = ((coded_width+31)&~31) >>4;
    mb_height = ((coded_height+31)&~31) >>4;
    twomb_line = (mb_height+1)>>1;
    recon_width = (mb_width+1)&0xfffffffe;
    twomb_width = (mb_width+1)>>1;
    if(twomb_line < 1 || twomb_width < 1)
    {
        LOGE("fatal error! twomb_line=%d, twomb_width=%d", twomb_line, twomb_width);
    }
    vdecbuf_width = twomb_width*32;
    vdecbuf_height = twomb_line*32;
    if(vdecbuf_width > coded_width)
    {
        nWidthMatchFlag = 0;
        if((vdecbuf_width - coded_width) != 16)
        {
            LOGW("(f:%s, l:%d) fatal error! vdecbuf_width=%d, gpubuf_width=%d,  the program will crash!", __FUNCTION__, __LINE__, vdecbuf_width, coded_width);
        }
        else
        {
            //LOGV("(f:%s, l:%d) Be careful! vdecbuf_width=%d, gpubuf_width=%d", __FUNCTION__, __LINE__, vdecbuf_width, coded_width);
        }
    }
    else if(vdecbuf_width == coded_width)
    {
        nWidthMatchFlag = 1;
    }
    else
    {
        LOGW("(f:%s, l:%d) fatal error! vdecbuf_width=%d <= gpubuf_width=%d, the program will crash!", __FUNCTION__, __LINE__, vdecbuf_width, coded_width);
        nWidthMatchFlag = 0;
    }
    for(i=0;i<twomb_line-1;i++)   //twomb line number
    {
        for(j=0;j<twomb_width-1;j++)   //macroblock(32*32) number in one line
        {
            for(l=0;l<16;l++)
            {
                //first mb
                m=i*16 + l;     //line num,skip oneline
                n= j*32;        //byte num in one line
                offset = m*coded_width + n;
                //memcpy(tarY+offset,ptr,32);
                dst_asm = tarY+offset;
                src_asm = ptr;
                asm volatile (
                        "vld2.8         {d0 - d1}, [%[src_asm]]!              \n\t"
                        "vld2.8         {d2 - d3}, [%[src_asm]]               \n\t"
                        "vswp           d0, d1                                \n\t"
                        "vswp           d2, d3                                \n\t"
                        "vst2.8         {d0 - d1}, [%[dst_asm]]!              \n\t"
                        "vst2.8         {d2 - d3}, [%[dst_asm]]               \n\t"
                        : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                        :  //[srcY] "r" (srcY)
                        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                        );

                ptr += 32;  //32 byte in one process.
                ptr += 32;  //32 byte in one process.
            }
        }
        //process last macroblock of one line, gpu buf must be 16byte align or 32 byte align
        { //last mb of one line
            for(l=0;l<16;l++)
            {
                //first mb
                m=i*16 + l;     //line num
                n= j*32;        //byte num in one line
                offset = m*coded_width + n;
                //memcpy(tarY+offset,ptr,32);
                dst_asm = tarY+offset;
                src_asm = ptr;
                if(nWidthMatchFlag)
                {
                    asm volatile (
                        "vld2.8         {d0 - d1}, [%[src_asm]]!              \n\t"
                        "vld2.8         {d2 - d3}, [%[src_asm]]               \n\t"
                        "vswp           d0, d1                                \n\t"
                        "vswp           d2, d3                                \n\t"
                        "vst2.8         {d0 - d1}, [%[dst_asm]]!              \n\t"
                        "vst2.8         {d2 - d3}, [%[dst_asm]]               \n\t"
                            : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                            :  //[srcY] "r" (srcY)
                            : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                            );
                }
                else
                {
                    asm volatile (
                            "vld1.8         {d0,d1}, [%[src_asm]]              \n\t"
                            "vst1.8         {d0,d1}, [%[dst_asm]]              \n\t"
                            : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                            :  //[srcY] "r" (srcY)
                            : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                            );
                }

                ptr += 32;  //32 byte in one process.
                ptr += 32;  //32 byte in one process.
            }
        }
    }
    //last twomb line, we process it alone
    nLeftValidLine = coded_height - (twomb_line-1)*32;
    if(nLeftValidLine!=32)
    {
        //LOGV("(f:%s, l:%d)hehehaha,gpuBufHeight[%d] is not 32 align", __FUNCTION__, __LINE__, nLeftValidLine);
    }
    for(j=0;j<twomb_width-1;j++)   //macroblock(32*32) number in one line
    {
        for(l=0;l<nLeftValidLine/2;l++)
        {
            //first mb
            m=i*16 + l;     //line num
            n= j*32;        //byte num in one line
            offset = m*coded_width + n;
            //memcpy(tarY+offset,ptr,32);
            dst_asm = tarY+offset;
            src_asm = ptr;
            asm volatile (
                "vld2.8         {d0 - d1}, [%[src_asm]]!              \n\t"
                "vld2.8         {d2 - d3}, [%[src_asm]]               \n\t"
                "vswp           d0, d1                                \n\t"
                "vswp           d2, d3                                \n\t"
                "vst2.8         {d0 - d1}, [%[dst_asm]]!              \n\t"
                "vst2.8         {d2 - d3}, [%[dst_asm]]               \n\t"
                    : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                    :  //[srcY] "r" (srcY)
                    : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                    );

            ptr += 32;  //32 byte in one process.
            ptr += 32;  //32 byte in one process.
        }
        ptr += (32-nLeftValidLine)*32;
    }
    //process last macroblock of last line, gpu buf must be 16byte align or 32 byte align
    { //last mb of last line
        for(l=0;l<nLeftValidLine/2;l++)
        {
            //first mb
            m=i*16 + l;     //line num
            n= j*32;        //byte num in one line
            offset = m*coded_width + n;
            //memcpy(tarY+offset,ptr,32);
            dst_asm = tarY+offset;
            src_asm = ptr;
            if(nWidthMatchFlag)
            {
                asm volatile (
                    "vld2.8         {d0 - d1}, [%[src_asm]]!              \n\t"
                    "vld2.8         {d2 - d3}, [%[src_asm]]               \n\t"
                    "vswp           d0, d1                                \n\t"
                    "vswp           d2, d3                                \n\t"
                    "vst2.8         {d0 - d1}, [%[dst_asm]]!              \n\t"
                    "vst2.8         {d2 - d3}, [%[dst_asm]]               \n\t"
                        : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                        :  //[srcY] "r" (srcY)
                        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                        );
            }
            else
            {
                asm volatile (
                        "vld1.8         {d0,d1}, [%[src_asm]]              \n\t"
                        "vst1.8         {d0,d1}, [%[dst_asm]]              \n\t"
                        : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
                        :  //[srcY] "r" (srcY)
                        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                        );
            }

            ptr += 32;  //32 byte in one process.
            ptr += 32;  //32 byte in one process.
        }
        ptr += (32-nLeftValidLine)*32;
    }
}

static void yuv420_mb_to_NV12(unsigned char* srcY, unsigned char* tarY,
                                unsigned char* srcC, unsigned char* tarC,
                                unsigned int width,unsigned int height)
{
    //convert y componet
    map32x32_to_yuv(srcY, tarY, width, height);

    //convert c componet
    map32x32_to_yuv(srcC, tarC, width, height>>1);
}

static void yuv420_mb_to_NV21(unsigned char* srcY, unsigned char* tarY,
                                unsigned char* srcC, unsigned char* tarC,
                                unsigned int width,unsigned int height)
{
    //convert y componet
    map32x32_to_yuv(srcY, tarY, width, height);

    //convert c componet
    //map32x32_to_yuv(srcC, tarC-1, width, height>>1);
    map32x32_to_yuv_nv21(srcC, tarC, width, height>>1);
    //map32x32_to_yuv_skip_oneline_nv21(srcC, tarC, width, height/2);
}


static void yuv422_mb_to_NV12(unsigned char* srcY, unsigned char* tarY,
                                unsigned char* srcC, unsigned char* tarC,
                                unsigned int width,unsigned int height)
{
    //convert y componet
    map32x32_to_yuv(srcY, tarY, width, height);

    //convert c componet
    map32x32_to_yuv_skip_oneline(srcC, tarC, width, height);
}

static void yuv422_mb_to_NV21(unsigned char* srcY, unsigned char* tarY,
                                unsigned char* srcC, unsigned char* tarC,
                                unsigned int width,unsigned int height)
{
    //convert y componet
    map32x32_to_yuv(srcY, tarY, width, height);

    //convert c componet
    map32x32_to_yuv_skip_oneline_nv21(srcC, tarC, width, height);
}


static void YUYVToNV21(const void* y,const void *u, void *nv21, int width, int height)
{
    int i,j;

    uint8_t* Y  = (uint8_t*)nv21;
    uint8_t* VU = (uint8_t*)Y + width * height;

    uint8_t* yuyv = (uint8_t)malloc(2*width * height*sizeof(uint8_t));
    memcpy(yuyv,y,width * height);
    memcpy(yuyv+width * height,u,width * height);

    for(i = 0; i < height; i += 2)   {
        for (j = 0; j < width; j++) {
            *(uint8_t*)((uint8_t*)Y + i * width + j) = *(uint8_t*)((uint8_t*)yuyv + i * width * 2 + j * 2);
            *(uint8_t*)((uint8_t*)Y + (i + 1) * width + j) = *(uint8_t*)((uint8_t*)yuyv + (i + 1) * width * 2 + j * 2);

            if (j % 2)  {
                if (j < width - 1) {
                    *(uint8_t*)((uint8_t*)VU + ((i * width) >> 1) + j) = *(uint8_t*)((uint8_t*)yuyv + i * width * 2 + (j + 1) * 2 + 1);
                }
            }
            else    {
                if (j > 1) {
                    *(uint8_t*)((uint8_t*)VU + ((i * width) >> 1) + j) = *(uint8_t*)((uint8_t*)yuyv + i * width * 2 + (j - 1) * 2 + 1);
                }
            }
        }
    }

    free(yuyv);
}

static void YUYVToYU12(const void* yuyv, void *yu12, int width, int height)
{
    int i,j;
    uint8_t* Y  = (uint8_t*)yu12;
    uint8_t* U  = (uint8_t*)Y + width * height;
    uint8_t* V      = (uint8_t*)Y + width*height*5/4;

    for(i = 0; i < height; i += 2)
    {
        for (j = 0; j < width; j++) {
            *(uint8_t*)((uint8_t*)Y + i * width + j) = *(uint8_t*)((uint8_t*)yuyv + i * width * 2 + j * 2);

            *(uint8_t*)((uint8_t*)Y + (i + 1) * width + j) = *(uint8_t*)((uint8_t*)yuyv + (i + 1) * width * 2 + j * 2);

            *(uint8_t*)((uint8_t*)U + ((i * width)/4) + j) = *(uint8_t*)((uint8_t*)yuyv + i * width * 2 + j * 4 + 1);

            *(uint8_t*)((uint8_t*)V + ((i * width)/4) + j) = *(uint8_t*)((uint8_t*)yuyv + i * width * 2 + j * 4 + 3);

        }
    }
}

static void YU12ToNV12(const void* yu12, void *nv12, int width, int height)
{
    int i;

    char * src_u    = (char *)yu12+ width * height;
    char * src_v    = (char *)yu12 + 5*width * height/4;
    char * dst_uv  = (char *)nv12+ width * height;

    memcpy(nv12, yu12, width * height);//copy Y

    for(i = 0; i < width * height/4; i ++)  {
        * (dst_uv++) = * (src_u+i);
        * (dst_uv++) = * (src_v+i);

    }
    //LOGE("Mic YU12 to NV12");
}

static void YU12ToNV21(const void* yu12, void *nv12, int width, int height)
{
    int i;

    char * src_u    = (char *)yu12+ width * height;
    char * src_v    = (char *)yu12 + 5*width * height/4;
    char * dst_uv  = (char *)nv12+ width * height;

    memcpy(nv12, yu12, width * height);//copy Y

    for(i = 0; i < width * height/4; i ++)  {
        * (dst_uv++) = * (src_v+i);
        * (dst_uv++) = * (src_u+i);
     }
    //LOGE("Mic YU12 to NV21");
}

static int MBToNV(const void* y,const void *u, void *nv12, int width, int height,cedarv_pixel_format_e informat, char *outformat)
{
    int ret;
    ScalerParameter cdx_scaler_para;
    cdx_scaler_para.addr_y_in = y;
    cdx_scaler_para.addr_c_in=  u;

    cdx_scaler_para.width_in = width;
    cdx_scaler_para.height_in = height;

    cdx_scaler_para.width_out = width;
    cdx_scaler_para.height_out = height;

    #ifdef USE_ION_MEM_ALLOCATOR
    unsigned int temp_y =(unsigned int)ion_alloc_alloc(width*height*3/2);
    unsigned int temp_u;
    unsigned int temp_v;
    int phy_add_y = ion_alloc_vir2phy((void*)temp_y);

    cdx_scaler_para.addr_y_out = phy_add_y;
    cdx_scaler_para.addr_u_out = phy_add_y+ width*height;
    cdx_scaler_para.addr_v_out = phy_add_y+ width*height*5/4;

    ion_flush_cache((void*)temp_y, width*height*3/2);

    #else USE_SUNXI_MEM_ALLOCATOR
    unsigned int temp_y =(unsigned int)sunxi_alloc_alloc(width*height*3/2);
    unsigned int temp_u;
    unsigned int temp_v;
    int phy_add_y = sunxi_alloc_vir2phy((void*)temp_y);

    cdx_scaler_para.addr_y_out = phy_add_y;
    cdx_scaler_para.addr_u_out = phy_add_y+ width*height;
    cdx_scaler_para.addr_v_out = phy_add_y+ width*height*5/4;

    sunxi_flush_cache((void*)temp_y, width*height*3/2);

    #endif

    LOGV(" hardwarePictureScaler");
    switch (informat){
        case CEDARV_PIXEL_FORMAT_MB_UV_COMBINE_YUV422 :
            //ret=HardwarePictureScaler(&cdx_scaler_para,DISP_FORMAT_YUV422);
            break;
        case CEDARV_PIXEL_FORMAT_MB_UV_COMBINE_YUV420 :
            //ret=HardwarePictureScaler(&cdx_scaler_para,DISP_FORMAT_YUV420);
            break;
        default :
            LOGE("The format can be supported now!!");
    }

    if(ret == 0){
        LOGD(" hardwarePictureScaler finished!!");
        memcpy(nv12,temp_y,width*height*1.5);
        //savefile("/mnt/sdcard/h264.yuv",temp_y,width*height*1.5);
        if(!strcmp(outformat,"NV12")){
            YU12ToNV12(temp_y,nv12,width,height);
            LOGE("Mic YU12 to NV12");
            }
        else if(!strcmp(outformat,"NV21")){
            YU12ToNV21(temp_y,nv12,width,height);
            LOGE("Mic YU12 to NV21");
            }
        else LOGE("the out format :%s can't be support now!!!",outformat);
        //savefile("/mnt/sdcard/nv12.yuv",nv12,width*height*1.5);*/
    }

    #ifdef USE_ION_MEM_ALLOCATOR
    ion_flush_cache((void*)nv12, width*height*3/2);
    ion_alloc_free((void*)temp_y);

    #else USE_SUNXI_MEM_ALLOCATOR
    sunxi_flush_cache((void*)nv12, width*height*3/2);
    sunxi_alloc_free((void*)temp_y);
    #endif

    return 0;
}

static int GetStreamData(void* in, u8* buf0, u32 buf_size0, u8* buf1, u32 buf_size1, cedarv_stream_data_info_t* data_info)
{
    LOGD("Starting get stream data!!");
    if(data_info->lengh <= buf_size0) {
            LOGV("The stream lengh is %d, the buf_size0 is %d",data_info->lengh,buf_size0);
            memcpy(buf0, in, data_info->lengh);
    }
    else {
        if(data_info->lengh <= (buf_size0+buf_size1)){
            LOGV("The stream lengh is %d, the buf_size0 is %d,the buf_size1 is %d",data_info->lengh,buf_size0,buf_size1);
            memcpy(buf0, in, buf_size0);
            memcpy(buf1,(in+buf_size0),(data_info->lengh -buf_size0));
        }
        else
            return -1;
    }
    data_info->flags |= CEDARV_FLAG_FIRST_PART;
    data_info->flags |= CEDARV_FLAG_LAST_PART;
    data_info->flags |= CEDARV_FLAG_PTS_VALID;

    return 0;
}

static int YUV_StreamOut(cedarv_picture_t *picture, void* out, char* format)
{
    u8* y_vir;
    u8* u_vir;
    u8* v_vir;
    LOGV("Stream out!!");

    u8* out_y = (u8*)out;
    u8* out_c = (u8*)out + picture->display_width*picture->display_height;

    LOGV("The picture->width is %d",picture->width);
    LOGV("The picture->height is %d",picture->height);

    y_vir=picture->y;
    u_vir=picture->u;

    LOGV("The picture y_vir addr is 0x%x",y_vir);
    LOGV("The picture u_vir addr is 0x%x",u_vir);

    //LOGE("Mic picture->pixel_format= %x",picture->pixel_format);

    switch(picture->pixel_format){
    case CEDARV_PIXEL_FORMAT_PLANNER_YUV420:
        LOGV("The decorder format is CEDARV_PIXEL_FORMAT_PLANNER_YUV420");
        //goto function
        break;

    case CEDARV_PIXEL_FORMAT_PLANNER_YVU420:
        LOGV("The decorder format is CEDARV_PIXEL_FORMAT_PLANNER_YVU420");

        #ifdef USE_ION_MEM_ALLOCATOR
        y_vir=ion_alloc_phy2vir(picture->y);
        #else USE_SUNXI_MEM_ALLOCATOR
        y_vir=sunxi_alloc_phy2vir(picture->y);
        #endif
        YU12ToNV21(y_vir,out,picture->display_width,picture->display_height);
        //memcpy(out,y_vir,(picture->display_width*picture->display_height)*3/2);
        //goto function
        //savefile("/mnt/sdcard/planner_yvu420.yuv",y_vir,(picture->display_width*picture->display_height)*3/2);
        break;

    case CEDARV_PIXEL_FORMAT_MB_UV_COMBINE_YUV422:
        LOGV("The decorder format is CEDARV_PIXEL_FORMAT_MB_UV_COMBINE_YUV422");
#if 0
        //goto function

        LOGV("MB_UV_COMBINE_YUV422 Changing to %s",format);
        MBToNV((void*)y_vir,(void*)u_vir,out,picture->width,picture->height,CEDARV_PIXEL_FORMAT_MB_UV_COMBINE_YUV422,format);

#else
        LOGD("width: %d, height: %d", picture->display_width, picture->display_height);

        #ifdef USE_ION_MEM_ALLOCATOR
        y_vir=ion_alloc_phy2vir(picture->y);
        u_vir=ion_alloc_phy2vir(picture->u);

        #else USE_SUNXI_MEM_ALLOCATOR
        y_vir=sunxi_alloc_phy2vir(picture->y);
        u_vir=sunxi_alloc_phy2vir(picture->u);
        #endif

        if(!strcmp(format,"NV12")){
            yuv422_mb_to_NV12(y_vir, out_y, u_vir, out_c,picture->display_width,picture->display_height);
        }else if(!strcmp(format,"NV21")){
            yuv422_mb_to_NV21(y_vir, out_y, u_vir, out_c,picture->display_width,picture->display_height);
            //LOGE("the out format :%s support now!!!",format);
        }else{
            LOGE("the out format :%s can't be support now!!!",format);
        }
#endif
        break;

    case CEDARV_PIXEL_FORMAT_MB_UV_COMBINE_YUV411:
        LOGV("The decorder format is CEDARV_PIXEL_FORMAT_MB_UV_COMBINE_YUV411");
        //goto function
        break;
    case CEDARV_PIXEL_FORMAT_MB_UV_COMBINE_YUV420:

        LOGV("The decorder format is CEDARV_PIXEL_FORMAT_MB_UV_COMBINE_YUV420");
#if 0
        //goto function
        LOGV("MB_UV_COMBINE_YUV420 Changing to %s",format);
        MBToNV((void*)y_vir,(void*)u_vir,out,picture->width,picture->height,CEDARV_PIXEL_FORMAT_MB_UV_COMBINE_YUV420,format);

#else

       #ifdef USE_ION_MEM_ALLOCATOR
        y_vir=ion_alloc_phy2vir(picture->y);
        u_vir=ion_alloc_phy2vir(picture->u);
       #else USE_SUNXI_MEM_ALLOCATOR
        y_vir=sunxi_alloc_phy2vir(picture->y);
        u_vir=sunxi_alloc_phy2vir(picture->u);
       #endif
        yuv420_mb_to_NV21(y_vir, out_y, u_vir,out_c,picture->display_width,picture->display_height);
#endif
        break;
    default:
        LOGE("The decorder format is no supported in out stream");
        return -1;
    }
    return 0;
}


void Libve_dec(cedarv_decoder_t** decoder,const void *in,void *out,void *decorder_stream_info,void *decorder_data_info,char* outfmt,ve_mutex_t *decorder_mutex)
{
    int            ret;

    u8*     buf0;
    u8*     buf1;
    u32     buf0size;
    u32     buf1size;

    cedarv_stream_data_info_t   *data_info  =(cedarv_stream_data_info_t *)decorder_data_info;
    cedarv_stream_info_t        *stream_info =(cedarv_stream_info_t*)decorder_stream_info;
    cedarv_picture_t          picture;

    ret = (*decoder)->request_write(*decoder, data_info->lengh, &buf0, &buf0size, &buf1, &buf1size);
    if(ret < 0){
        //* request bit stream data buffer fail, may be the bit stream FIFO is full.
        //* in this case, we should call decoder->decode(...) to decode stream data and release bit stream buffer.
        //* here we just use a single thread to do the data parsing/decoding/picture requesting work, so it is
        //* invalid to see that the bit stream FIFO is full.
        LOGE("request bit stream buffer fail.\n");
        goto fail;
    }

    LOGV("GetStreamData!!");
    GetStreamData(in,buf0,buf0size,buf1,buf1size,data_info);

    //* tell libcedarv stream data has been added.
    (*decoder)->update_data(*decoder, data_info);       //* this decoder operation do not use hardware, so need not lock the mutex.

    //* decode bit stream data.

    decorder_mutex_lock(decorder_mutex);
    ret = (*decoder)->decode(*decoder);
    decorder_mutex_unlock(decorder_mutex);

    if(ret == CEDARV_RESULT_ERR_NO_MEMORY || ret == CEDARV_RESULT_ERR_UNSUPPORTED){
        LOGE("bit stream is unsupported.\n");
        goto fail;
    }
    if(ret==CEDARV_RESULT_OK)
        LOGV("Successfully return,decording!!");
    if(ret==CEDARV_RESULT_FRAME_DECODED)
        LOGV("One frame decorded!!");
    if(ret == CEDARV_RESULT_KEYFRAME_DECODED)
        LOGV("One key frame decorded!!");

    if (ret == CEDARV_RESULT_FRAME_DECODED ||           \
        ret == CEDARV_RESULT_KEYFRAME_DECODED ||    \
        ret == CEDARV_RESULT_NO_FRAME_BUFFER)
    {
        //* request picture from libcedarv.

        ret = (*decoder)->display_request(*decoder, &picture);      //* this decoder operation do not use hardware, so need not lock the mutex.

        LOGV("The return of display_request is %d",ret);

        if(ret == 0){
            //* get one picture from decoder success, do some process work on this picture.
            LOGV("Copy data to out");

            decorder_mutex_lock(decorder_mutex);
            YUV_StreamOut(&picture,out,outfmt);
            decorder_mutex_unlock(decorder_mutex);
            stream_info->video_width = picture.width;
            stream_info->video_height = picture.height;
            //* release the picture to libcedarv.
            LOGV("decoder->display_release");
            (*decoder)->display_release(*decoder, picture.id);      //* this decoder operation do not use hardware, so need not lock the mutex.
        }

    }

    if(stream_info->init_data)
        free(stream_info->init_data);
    return ;
fail:

    LOGE("Fail operations");
    decorder_mutex_lock(decorder_mutex);
    (*decoder)->ioctrl(*decoder, CEDARV_COMMAND_STOP, 0);
    decorder_mutex_unlock(decorder_mutex);

    decorder_mutex_lock(decorder_mutex);
    (*decoder)->close(*decoder);
    libcedarv_exit(*decoder);
    decorder_mutex_unlock(decorder_mutex);

    if(stream_info->init_data)
        free(stream_info->init_data);

    cedarx_hardware_exit(0);
    decorder_mutex_destroy(decorder_mutex);

    return ;

}

int Libve_init(cedarv_decoder_t** decoder,cedarv_stream_info_t* stream_info,int scale,ve_mutex_t *decorder_mutex)
{
    int ret;
    LOGD("Libve_init!!");

#if VE_MUTEX_ENABLE
    if (decorder_mutex_init(decorder_mutex, CEDARV_DECODE) < 0) {
        LOGE("ve mutex init fail!!");
        return -1;
    }
#endif

    *decoder =NULL;
    cedarx_hardware_init(0);
    decorder_mutex_lock(decorder_mutex);
    *decoder = libcedarv_init(&ret);
    decorder_mutex_unlock(decorder_mutex);

    LOGD("libcedarv_init!!");
    if(ret < 0){
        LOGE("can not initialize the decoder library.\n");
        return -1;
    }
    LOGV("decoder->set_vstream_info!!");
    (*decoder)->set_vstream_info(*decoder, stream_info);        //* this decoder operation do not use hardware, so need not lock the mutex.

    decorder_mutex_lock(decorder_mutex);
    (*decoder)->ioctrl(*decoder, CEDARV_COMMAND_SET_PIXEL_FORMAT, 0);
    decorder_mutex_unlock(decorder_mutex);

    LOGD("decoder->open!!");
    decorder_mutex_lock(decorder_mutex);
    if(scale && stream_info->video_width == 1920 &&  stream_info->video_height ==1080)
    {
        (*decoder)->ioctrl(*decoder, CEDARV_COMMAND_SET_MAX_OUTPUT_WIDTH, stream_info->video_width/2);
        (*decoder)->ioctrl(*decoder, CEDARV_COMMAND_SET_MAX_OUTPUT_HEIGHT, stream_info->video_height/2);
    }
    ret =(*decoder)->open(*decoder);
    decorder_mutex_unlock(decorder_mutex);

    if(ret < 0){
        LOGE("can not open decoder.\n");
        if(stream_info->init_data)
            free(stream_info->init_data);

        decorder_mutex_lock(decorder_mutex);
        libcedarv_exit(*decoder);
        decorder_mutex_unlock(decorder_mutex);

        return (void*)-1;
    }

    decorder_mutex_lock(decorder_mutex);
    (*decoder)->ioctrl(*decoder, CEDARV_COMMAND_PLAY, 0);
    decorder_mutex_unlock(decorder_mutex);

    ret=0;

    return ret;
}

int Libve_exit(cedarv_decoder_t** decoder,ve_mutex_t *decorder_mutex)
{
    LOGD("Libve_exit!!");

    decorder_mutex_lock(decorder_mutex);
    (*decoder)->ioctrl(*decoder, CEDARV_COMMAND_STOP, 0);
    decorder_mutex_unlock(decorder_mutex);

    decorder_mutex_lock(decorder_mutex);
    (*decoder)->close(*decoder);
    libcedarv_exit(*decoder);
    decorder_mutex_unlock(decorder_mutex);
    cedarx_hardware_exit(0);
#if VE_MUTEX_ENABLE
    decorder_mutex_destroy(decorder_mutex);
#endif

    return 0;
}
