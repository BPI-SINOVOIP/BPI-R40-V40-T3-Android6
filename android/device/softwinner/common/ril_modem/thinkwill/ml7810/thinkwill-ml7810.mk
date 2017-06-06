# 3G Modem Configuration Flie

PRODUCT_COPY_FILES += \
	device/softwinner/common/ril_modem/thinkwill/ml7810/libreference-ril-ml7810.so:system/lib/libreference-ril-ml7810.so \
	device/softwinner/common/ril_modem/thinkwill/ml7810/init.dhcpcd-rndis:system/etc/init.dhcpcd-rndis \
	device/softwinner/common/ril_modem/thinkwill/ml7810/init.rndis-down:system/etc/init.rndis-down
	
PRODUCT_COPY_FILES += \
	device/softwinner/common/ril_modem/thinkwill/ml7810/init.thinkwill.rc:root/init.sunxi.3g.rc
