$(call inherit-product, build/target/product/full_base.mk)
$(call inherit-product, device/softwinner/t3-common/t3-common.mk)
$(call inherit-product-if-exists, device/softwinner/t3-p3/modules/modules.mk)

DEVICE_PACKAGE_OVERLAYS := device/softwinner/t3-p3/overlay \
                           $(DEVICE_PACKAGE_OVERLAYS)

PRODUCT_PACKAGES += Launcher3

PRODUCT_PACKAGES += \
    ESFileExplorer \
    VideoPlayer \
    Bluetooth
#   PartnerChromeCustomizationsProvider

PRODUCT_COPY_FILES += \
    device/softwinner/t3-p3/kernel:kernel \
    device/softwinner/t3-p3/fstab.sun8iw11p1:root/fstab.sun8iw11p1 \
    device/softwinner/t3-p3/init.sun8iw11p1.rc:root/init.sun8iw11p1.rc \
    device/softwinner/t3-p3/init.recovery.sun8iw11p1.rc:root/init.recovery.sun8iw11p1.rc \
    device/softwinner/t3-p3/ueventd.sun8iw11p1.rc:root/ueventd.sun8iw11p1.rc \
    device/softwinner/common/verity/rsa_key/verity_key:root/verity_key \
    device/softwinner/t3-p3/modules/modules/nand.ko:root/nand.ko \
    device/softwinner/t3-p3/modules/modules/gt82x.ko:root/gt82x.ko \
    device/softwinner/t3-p3/modules/modules/sw-device.ko:root/sw-device.ko

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.camera.xml:system/etc/permissions/android.hardware.camera.xml \
    frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
    frameworks/native/data/etc/android.hardware.bluetooth.xml:system/etc/permissions/android.hardware.bluetooth.xml \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:system/etc/permissions/android.hardware.bluetooth_le.xml \
    frameworks/native/data/etc/android.software.verified_boot.xml:system/etc/permissions/android.software.verified_boot.xml \
    frameworks/native/data/etc/android.hardware.ethernet.xml:system/etc/permissions/android.hardware.ethernet.xml

PRODUCT_COPY_FILES += \
    device/softwinner/t3-p3/configs/camera.cfg:system/etc/camera.cfg \
    device/softwinner/t3-p3/configs/gsensor.cfg:system/usr/gsensor.cfg \
    device/softwinner/t3-p3/configs/media_profiles.xml:system/etc/media_profiles.xml \
    device/softwinner/t3-p3/configs/sunxi-keyboard.kl:system/usr/keylayout/sunxi-keyboard.kl \
    device/softwinner/t3-p3/configs/tp.idc:system/usr/idc/tp.idc
	
PRODUCT_COPY_FILES += \
	device/softwinner/t3-p3/trd/wb.ko:system/lib/wb.ko \
	device/softwinner/t3-p3/trd/config.ini:system/config.ini \
	device/softwinner/t3-p3/trd/gocsdk:system/bin/gocsdk \
	device/softwinner/t3-p3/trd/rtl8723b_fw:system/etc/firmware/rtl8723b_fw \
	device/softwinner/t3-p3/trd/rtl8723b_config:system/etc/firmware/rtl8723b_config \
	device/softwinner/t3-p3/trd/ring.mp3:system/ring.mp3 \
	device/softwinner/t3-p3/trd/8723bu.ko:system/lib/8723bu.ko \
	device/softwinner/t3-p3/trd/libwapm.so:system/lib/libwapm.so \
	device/softwinner/t3-p3/trd/serial:system/bin/serial 

PRODUCT_COPY_FILES += \
    device/softwinner/t3-p3/hawkview/sensor_list_cfg.ini:system/etc/hawkview/sensor_list_cfg.ini

# bootanimation
PRODUCT_COPY_FILES += \
    device/softwinner/t3-p3/media/boot.wav:system/media/boot.wav
#    device/softwinner/t3-p3/media/bootanimation.zip:system/media/bootanimation.zip \
    

# Radio Packages and Configuration Flie

#BOARD_3G_MODEL_TYPE := thinkwill_ml7810

$(call inherit-product, device/softwinner/common/rild/radio_common.mk)
#$(call inherit-product, device/softwinner/common/ril_modem/huawei/mu509/huawei_mu509.mk)
#$(call inherit-product, device/softwinner/common/ril_modem/Oviphone/em55/oviphone_em55.mk)

ifeq ($(BOARD_3G_MODEL_TYPE),thinkwill_ml7810)
$(call inherit-product, device/softwinner/common/ril_modem/thinkwill/ml7810/thinkwill-ml7810.mk)
endif

# Realtek wifi efuse map
PRODUCT_COPY_FILES += \
    device/softwinner/t3-p3/wifi_efuse_8723bs-vq0.map:system/etc/wifi/wifi_efuse_8723bs-vq0.map

PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.usb.config=mtp,adb \
    ro.adb.secure=0 \
    rw.logger=0

PRODUCT_PROPERTY_OVERRIDES += \
    dalvik.vm.heapsize=512m \
    dalvik.vm.heapstartsize=8m \
    dalvik.vm.heapgrowthlimit=192m \
    dalvik.vm.heaptargetutilization=0.75 \
    dalvik.vm.heapminfree=2m \
    dalvik.vm.heapmaxfree=8m 

PRODUCT_PROPERTY_OVERRIDES += \
	persis.sys.bluetooth_goc=0
	
PRODUCT_PROPERTY_OVERRIDES += \
    ro.zygote.disable_gl_preload=true
	
PRODUCT_PROPERTY_OVERRIDES += \
    ro.sf.lcd_density=160 \

PRODUCT_PROPERTY_OVERRIDES += \
    ro.spk_dul.used=false \

PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.timezone=Asia/Shanghai \
    persist.sys.country=CN \
    persist.sys.language=zh

# stoarge
PRODUCT_PROPERTY_OVERRIDES += \
    persist.fw.force_adoptable=true

PRODUCT_PROPERTY_OVERRIDES += \
	ro.lockscreen.disable.default=true

PRODUCT_CHARACTERISTICS := tablet

PRODUCT_AAPT_CONFIG := tvdpi xlarge hdpi xhdpi large
PRODUCT_AAPT_PREF_CONFIG := tvdpi

#$(call inherit-product-if-exists, vendor/google/products/gms_base.mk)

PRODUCT_BRAND := Allwinner
PRODUCT_NAME := t3_p3
PRODUCT_DEVICE := t3-p3
PRODUCT_MODEL := QUAD-CORE T3 p3
PRODUCT_MANUFACTURER := Allwinner
PACK_BOARD := t3-p3
