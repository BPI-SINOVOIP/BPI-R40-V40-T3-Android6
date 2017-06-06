
MODULE_TOP = $(TOP)/frameworks/av/media/libcedarc

product = $(shell echo $(TARGET_PRODUCT) | cut -d '_' -f 1)

ifeq ($(product), octopus)
include $(MODULE_TOP)/config/config_pad_A83_lollipop.mk
endif

ifeq ($(product), tulip)
include $(MODULE_TOP)/config/config_pad_A64_lollipop.mk
endif

ifeq ($(product), dolphin)
include $(MODULE_TOP)/config/config_box_H3_kitkat.mk
endif

ifeq ($(product), rabbit)
include $(MODULE_TOP)/config/config_box_H64_lollipop.mk
endif

ifeq ($(product), cheetah)
include $(MODULE_TOP)/config/config_box_H5_lollipop.mk
endif

ifeq ($(product), eagle)
include $(MODULE_TOP)/config/config_box_H8_kitkat.mk
endif

ifeq ($(product), t3)
include $(MODULE_TOP)/config/config_cdr_T3_Marshmallow.mk
endif

ifeq ($(product), kylin)
include $(MODULE_TOP)/config/config_vr_A80_Nougat.mk
endif
