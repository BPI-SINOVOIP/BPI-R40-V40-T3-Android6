
include $(top_srcdir)/Makefile.inc

bin_PROGRAMS = demoVencoder

demoVencoder_SOURCES =  \
			EncoderTest.c

demoVencoder_CFLAGS = $(CFLAGS_CDXG)
demoVencoder_CPPFLAGS = $(CPPFLAGS_CDXG)
LOCAL_INCLUDE = -I$(top_srcdir) \
		-I$(top_srcdir)/libcore/include \
		-I$(top_srcdir)/libcore/parser/include \
		-I$(top_srcdir)/libcore/stream/include \
        -I$(top_srcdir)/external/include/adecoder/ \
		-I$(top_srcdir)/libcore/base/include \
		-I$(top_srcdir)/../libcedarc/include

demoVencoder_CFLAGS += $(LOCAL_INCLUDE)
demoVencoder_CPPFLAGS += $(LOCAL_INCLUDE)

demoVencoder_LDFLAGS = $(LDFLAGS_CDXG)

demoVencoder_LDADD = \
			$(top_srcdir)/libcore/base/libcdx_base.la 

LOCAL_LIB = -lpthread -lcdc_ve -lcdc_vencoder -lcdc_memory

demoVencoder_LDFLAGS += $(LOCAL_LIB)

