# inherit common.mk
$(call inherit-product, device/softwinner/common/common.mk)

DEVICE_PACKAGE_OVERLAYS := \
    device/softwinner/t3-common/overlay \
    $(DEVICE_PACKAGE_OVERLAYS)

PRODUCT_PACKAGES += \
    lights.t3 \
    hwcomposer.t3 \
    camera.t3

PRODUCT_PACKAGES += \
    libion

# audio
PRODUCT_PACKAGES += \
    audio.primary.t3 \
    audio.external.t3 \
    audio.a2dp.default \
    audio.usb.default \
    audio.r_submix.default

PRODUCT_PACKAGES +=\
    charger_res_images \
    charger

# Set zygote config

PRODUCT_COPY_FILES += \
    hardware/aw/audio/t3/audio_policy.conf:system/etc/audio_policy.conf \
    hardware/aw/audio/t3/phone_volume.conf:system/etc/phone_volume.conf \
    hardware/aw/audio/t3/codec_paths.xml:system/etc/codec_paths.xml

PRODUCT_COPY_FILES += \
    frameworks/av/media/libstagefright/data/media_codecs_google_audio.xml:system/etc/media_codecs_google_audio.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_telephony.xml:system/etc/media_codecs_google_telephony.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_video.xml:system/etc/media_codecs_google_video.xml

PRODUCT_COPY_FILES += \
    device/softwinner/t3-common/media_codecs.xml:system/etc/media_codecs.xml \
    device/softwinner/t3-common/media_codecs_performance.xml:system/etc/media_codecs_performance.xml \
    device/softwinner/t3-common/cts_media_codecs.xml:system/etc/cts_media_codecs.xml \
    device/softwinner/t3-common/init.sun8iw11p1.usb.rc:root/init.sun8iw11p1.usb.rc \
    device/softwinner/t3-common/configs/cfg-videoplayer.xml:system/etc/cfg-videoplayer.xml

PRODUCT_COPY_FILES += \
    device/softwinner/common/config/android.hardware.sensor.temperature.ambient.xml:system/etc/permissions/android.hardware.sensor.temperature.ambient.xml

#------------------ watermark -------------------

PRODUCT_COPY_FILES += \
    $(call find-copy-subdir-files,*.bmp,device/softwinner/common/hardware/camera/allwinnertech/water_mark/watermark,system/watermark)

#------------------------------------------------

# video libs
PRODUCT_PACKAGES += \
  libMemAdapter          \
	libcdx_base              \
	libcdx_stream            \
	libnormal_audio          \
	libcdx_parser            \
	libVE                    \
	libvdecoder              \
	libvencoder              \
	libadecoder              \
	libsdecoder              \
	libplayer                \
	libad_audio              \
	libaw_plugin             \
	libthumbnailplayer       \
	libaw_wvm                \
	libstagefrighthw         \
	libOmxCore               \
	libOmxVdec               \
	libOmxVenc               \
	libI420colorconvert      \
	libawmetadataretriever   \
	libawplayer		 \
	libawh264		\
	libawh265		\
	libawmjpeg		\
	libawmjpegplus		\
	libawmpeg2		\
	libawmpeg4base		\
	libawvp6soft		\
	libawvp8		\
	libawwmv3  		\
	libawvp9soft		\
	libawh265soft		\
	libawmpeg4h263		\
	libawmpeg4vp6		\
	libawmpeg4normal	\
	libawmpeg4dx    	\
	libawwmv12soft 		\
	libawavs

# egl
#------------------ egl -------------------------
PRODUCT_COPY_FILES += \
    device/softwinner/t3-common/egl/egl.cfg:system/lib/egl/egl.cfg \
    device/softwinner/t3-common/egl/gralloc.sunxi.so:system/lib/hw/gralloc.sun8iw11p1.so \
    device/softwinner/t3-common/egl/libGLES_mali.so:system/lib/egl/libGLES_mali.so


PRODUCT_PROPERTY_OVERRIDES += \
    wifi.interface=wlan0 \
    wifi.supplicant_scan_interval=15 \
    keyguard.no_require_sim=true

PRODUCT_PROPERTY_OVERRIDES += \
    ro.kernel.android.checkjni=0

# 131072=0x20000 196608=0x30000
PRODUCT_PROPERTY_OVERRIDES += \
    ro.opengles.version=131072

# For mali GPU only
PRODUCT_PROPERTY_OVERRIDES += \
    debug.hwui.render_dirty_regions=false

PRODUCT_PROPERTY_OVERRIDES += \
	persist.sys.strictmode.visual=0 \
	persist.sys.strictmode.disable=1

PRODUCT_PROPERTY_OVERRIDES += \
    ro.sys.cputype=QuadCore-T3

# Enabling type-precise GC results in larger optimized DEX files.  The
# additional storage requirements for ".odex" files can cause /system
# to overflow on some devices, so this is configured separately for
# each product.
PRODUCT_TAGS += dalvik.gc.type-precise

PRODUCT_PROPERTY_OVERRIDES += \
    ro.product.firmware=V1.0

# if DISPLAY_BUILD_NUMBER := true then
# BUILD_DISPLAY_ID := $(BUILD_ID).$(BUILD_NUMBER)
# required by gms.
DISPLAY_BUILD_NUMBER := true
BUILD_NUMBER := $(shell date +%Y%m%d)

# widevine
BOARD_WIDEVINE_OEMCRYPTO_LEVEL := 1
SECURE_OS_OPTEE := no

#add widevine libraries
PRODUCT_PROPERTY_OVERRIDES += \
    drm.service.enabled=true \
    ro.sys.widevine_oemcrypto_level=1

PRODUCT_PACKAGES += \
    com.google.widevine.software.drm.xml \
    com.google.widevine.software.drm \
    libdrmwvmplugin \
    libwvm \
    libWVStreamControlAPI_L${BOARD_WIDEVINE_OEMCRYPTO_LEVEL} \
    libwvdrm_L${BOARD_WIDEVINE_OEMCRYPTO_LEVEL} \
    libdrmdecrypt \
    libwvdrmengine

ifeq ($(BOARD_WIDEVINE_OEMCRYPTO_LEVEL), 1)
    PRODUCT_PACKAGES += liboemcrypto \
                        libtee_client
endif
