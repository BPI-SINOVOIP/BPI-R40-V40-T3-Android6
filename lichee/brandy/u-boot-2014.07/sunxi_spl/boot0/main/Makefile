
##
## Makefile for Sunxi Secure Boot
##



include $(SPLDIR)/config.mk

LIB	:= $(obj)libmain.o

HEAD    := boot0_head.o

DATA    := boot0_hash.o
START	:= boot0_entry.o

ifdef CONFIG_SUNXI_MULITCORE_BOOT
COBJS   += boot0_secondary.o
COBJS   += boot0_main_smp.o
else
COBJS   += boot0_main.o
endif

SRCS	:= $(START:.o=.S) $(COBJS:.o=.c) $(HEAD:.o=.c)
OBJS	:= $(addprefix $(obj),$(COBJS) $(COBJS-y) $(SOBJS))
HEAD    := $(addprefix $(obj),$(HEAD))
START	:= $(addprefix $(obj),$(START))


all:	$(obj).depend $(HEAD) $(START) $(LIB) $(DATA)

$(LIB):	$(OBJS)
	$(call cmd_link_o_target, $(OBJS))


#########################################################################

# defines $(obj).depend target
include $(SRCTREE)/rules.mk

sinclude $(obj).depend

#########################################################################
