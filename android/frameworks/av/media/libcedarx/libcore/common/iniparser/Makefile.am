
include $(top_srcdir)/Makefile.inc

lib_LTLIBRARIES = libcdx_iniparser.la

libcdx_iniparser_la_SOURCES =  ./iniparser.c \
			./iniparserapi.c \
			./dictionary.c

libcdx_iniparser_la_CFLAGS = $(CFLAGS_CDXG)
LOCAL_INCLUDE = -I../include \
		-I../../include \
		-I$(top_srcdir)/

libcdx_iniparser_la_CFLAGS += $(LOCAL_INCLUDE)


libcdx_iniparser_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_iniparser_la_LIBADD = 

#LOCAL_LIB = -lpthread
libcdx_iniparser_la_LDFLAGS += $(LOCAL_LIB)

