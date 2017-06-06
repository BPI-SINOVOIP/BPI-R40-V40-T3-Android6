$(call inherit-product, build/target/product/full_base.mk)
$(call inherit-product, device/softwinner/t3-common/t3-common.mk)
$(call inherit-product-if-exists, device/softwinner/t3-p1/modules/modules.mk)

DEVICE_PACKAGE_OVERLAYS := device/softwinner/t3-p1/overlay \
                           $(DEVICE_PACKAGE_OVERLAYS)

PRODUCT_PACKAGES += Launcher3

PRODUCT_PACKAGES += \
    ESFileExplorer \
    VideoPlayer \
    Bluetooth
#   PartnerChromeCustomizationsProvider

PRODUCT_COPY_FILES += \
    device/softwinner/t3-p1/kernel:kernel \
    device/softwinner/t3-p1/fstab.sun8iw11p1:root/fstab.sun8iw11p1 \
    device/softwinner/t3-p1/init.sun8iw11p1.rc:root/init.sun8iw11p1.rc \
    device/softwinner/t3-p1/init.recovery.sun8iw11p1.rc:root/init.recovery.sun8iw11p1.rc \
    device/softwinner/t3-p1/ueventd.sun8iw11p1.rc:root/ueventd.sun8iw11p1.rc \
    device/softwinner/common/verity/rsa_key/verity_key:root/verity_key \
    device/softwinner/t3-p1/modules/modules/nand.ko:root/nand.ko \
    device/softwinner/t3-p1/modules/modules/gt82x.ko:root/gt82x.ko \
    device/softwinner/t3-p1/modules/modules/sw-device.ko:root/sw-device.ko

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.camera.xml:system/etc/permissions/android.hardware.camera.xml \
    frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
    frameworks/native/data/etc/android.hardware.bluetooth.xml:system/etc/permissions/android.hardware.bluetooth.xml \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:system/etc/permissions/android.hardware.bluetooth_le.xml \
    frameworks/native/data/etc/android.software.verified_boot.xml:system/etc/permissions/android.software.verified_boot.xml \
    frameworks/native/data/etc/android.hardware.ethernet.xml:system/etc/permissions/android.hardware.ethernet.xml

PRODUCT_COPY_FILES += \
    device/softwinner/t3-p1/configs/camera.cfg:system/etc/camera.cfg \
    device/softwinner/t3-p1/configs/gsensor.cfg:system/usr/gsensor.cfg \
    device/softwinner/t3-p1/configs/media_profiles.xml:system/etc/media_profiles.xml \
    device/softwinner/t3-p1/configs/sunxi-keyboard.kl:system/usr/keylayout/sunxi-keyboard.kl \
    device/softwinner/t3-p1/configs/tp.idc:system/usr/idc/tp.idc

PRODUCT_COPY_FILES += \
    device/softwinner/t3-p1/hawkview/sensor_list_cfg.ini:system/etc/hawkview/sensor_list_cfg.ini

# bootanimation
#PRODUCT_COPY_FILES += \
#    device/softwinner/t3-p1/media/bootanimation.zip:system/media/bootanimation.zip \
#    device/softwinner/t3-p1/media/boot.wav:system/media/boot.wav

# Radio Packages and Configuration Flie
$(call inherit-product, device/softwinner/common/rild/radio_common.mk)
#$(call inherit-product, device/softwinner/common/ril_modem/huawei/mu509/huawei_mu509.mk)
#$(call inherit-product, device/softwinner/common/ril_modem/Oviphone/em55/oviphone_em55.mk)

# Realtek wifi efuse map
PRODUCT_COPY_FILES += \
    device/softwinner/t3-p1/wifi_efuse_8723bs-vq0.map:system/etc/wifi/wifi_efuse_8723bs-vq0.map

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
    ro.zygote.disable_gl_preload=true

PRODUCT_PROPERTY_OVERRIDES += \
    ro.sf.lcd_density=160 \
    ro.display.sdcard=1

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
PRODUCT_NAME := t3_p1
PRODUCT_DEVICE := t3-p1
PRODUCT_MODEL := QUAD-CORE T3 p1
PRODUCT_MANUFACTURER := Allwinner
PACK_BOARD := t3-p1
