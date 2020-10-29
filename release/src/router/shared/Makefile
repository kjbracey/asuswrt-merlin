include ../common.mak

CFLAGS += -Os -Wall $(EXTRACFLAGS) -fPIC
CFLAGS += -I. -I$(TOP)/shared -I$(SRCBASE)/include
CFLAGS += -I$(LINUXDIR)/include
CFLAGS += -D__EXPORTED_HEADERS__

ifeq ($(RTCONFIG_SHP),y)
CFLAGS += -DSHP
endif

ifeq ($(RTCONFIG_RALINK),y)
CFLAGS += -I./sysdeps/ralink -I$(TOP)/wireless_tools/ -DWE_NOLIBM 
endif

ifeq ($(RTCONFIG_BCMWL6),y)
CFLAGS += -I$(SRCBASE)/shared/bcmwifi/include
CFLAGS += -I$(SRCBASE)/common/include
else
CFLAGS += -I$(SRCBASE)/shared
endif

ifeq ($(RTCONFIG_QTN),y)
CFLAGS += -I$(TOP)/libqcsapi_client
LDFLAGS += -L$(TOP)/libqcsapi_client -lqcsapi_client
endif

ifeq ($(RTCONFIG_BCMSMP),y)
CFLAGS += -DSMP
endif

ifeq ($(RTCONFIG_5301X),y)
CFLAGS += -DBCM5301X
endif

include $(SRCBASE)/.config

ifneq ($(findstring DSL,$(BUILD_NAME)),)
MODEL = $(subst -,_,$(BUILD_NAME))
else
MODEL = $(subst -,,$(BUILD_NAME))
endif

export CFLAGS += -D$(MODEL)

ifeq ($(RTCONFIG_4352_5G),y)
CFLAGS += -DBCM4352
endif

ifeq (2_6_36,$(LINUX_VERSION))
CFLAGS += -DAUTOFW_PORT_DEPRECATED
endif

CFLAGS += -DTRAFFIC_MGMT

OBJS = shutils.o linux_timer.o defaults.o model.o rtstate.o boardapi.o
OBJS += network_utility.o
OBJS += misc.o version.o files.o strings.o process.o 
OBJS += bin_sem_asus.o semaphore.o pids.o notify_rc.o discover.o
OBJS += base64.o
OBJS += nvparse.o
OBJS += scripts.o

ifeq ($(RTCONFIG_RALINK),y)
OBJS += $(TOP)/wireless_tools/iwlib.o
OBJS += flash_mtd.o api-ralink.o
ifeq ($(RTCONFIG_RALINK_RT3052),y)
OBJS += ra3052.o
endif
ifeq ($(RTN14U)$(RTAC52U)$(RTAC51U)$(RTN11P),y)
CFLAGS += -DRA_ESW
OBJS += mt7620.o
else
OBJS += rtkswitch.o
endif
else
OBJS += wl.o wl_linux.o
OBJS += broadcom.o api-broadcom.o
ifeq ($(RTCONFIG_DSL_TCLINUX),y)
OBJS += api-tclinux.o
endif
endif

ifeq ($(RTCONFIG_USB),y)
OBJS += usb.o
ID_CFLAGS += -I$(TOP)/busybox/include
ID_CFLAGS += -I$(TOP)/busybox/util-linux/volume_id
ID_CFLAGS += -include $(TOP)/busybox/include/autoconf.h
ID_CFLAGS += -D_GNU_SOURCE -DNDEBUG -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
#ID_CFLAGS += -fdata-sections -ffunction-sections
ID_OBJS  = $(TOP)/busybox/util-linux/volume_id/util.o
ID_OBJS += $(TOP)/busybox/util-linux/volume_id/linux_swap.o
ID_OBJS += $(TOP)/busybox/util-linux/volume_id/ext.o
ID_OBJS += $(TOP)/busybox/util-linux/volume_id/fat.o
ID_OBJS += $(TOP)/busybox/util-linux/volume_id/ntfs.o
ID_OBJS += $(if $(HFS),$(TOP)/busybox/util-linux/volume_id/hfs.o)
ID_OBJS += $(if $(HFS),$(patsubst %.c,%.o,$(wildcard $(TOP)/busybox/libbb/hash_md5.c)))
ID_OBJS += $(if $(HFS),$(patsubst %.c,%.o,$(wildcard $(TOP)/busybox/libbb/bb_bswap_64.c)))
ID_OBJS += volume_id.o
#OBJS += volume_id.lib.o
OBJS += $(notdir $(ID_OBJS))
endif

ifeq ($(RTCONFIG_BCMARM),y)
OBJS += ifaddrs.o
endif

vpath %.c sysdeps sysdeps/broadcom sysdeps/ralink $(SRCBASE)/shared/ $(SRCBASE)/shared/bcmwifi/src $(WLAN_ComponentSrcDirs)

all: libshared.so libshared.a

ifeq ($(CONFIG_BCMWL5),y)
OBJS += wlif_utils.o UdpLib.o bcmutils.o
OBJS += bcmwifi_channels.o
endif

libshared.so: $(OBJS)
	@echo " [shared] LD $@"
	@$(LD) $(LDFLAGS) -shared -o $@ $^

	$(SIZECHECK)
	$(CPTMP)

libshared.a: $(OBJS)
	@echo " [shared] AR $@"
	@rm -f $@
	@$(AR) cruv $@ $^


install: all
	@echo " [shared] Installing as $(INSTALLDIR)/usr/lib/libshared.so"
	@install -d $(INSTALLDIR)/usr/lib
	@install -m 755 libshared.so $(INSTALLDIR)/usr/lib
	@$(STRIP) $(INSTALLDIR)/usr/lib/libshared.so


clean:
	rm -f *.o *.so *.a .*.depend sysdeps/*.o sysdeps/broadcom/*.o sysdeps/ralink/*.o

ifeq ($(RTCONFIG_USB),y)
vpath %.c $(sort $(dir $(ID_OBJS)))

$(notdir $(ID_OBJS)): %.o: %.c .%.depend
	@echo " [shared] CC $@"
	@$(CC) $(CFLAGS) $(ID_CFLAGS) -o $@ -c $<

$(patsubst %.o,.%.depend,$(notdir $(ID_OBJS))): .%.depend: %.c
	@$(CC) $(CFLAGS) $(ID_CFLAGS) -M $< > $@

# ld -r --gc-sections is broken, so don't use it for now
#volume_id.lib.o: $(notdir $(ID_OBJS))
#	@echo " [shared] LD $@"
#	@$(LD) -r -e find_label_or_uuid --gc-sections -o $@ $(notdir $(ID_OBJS))
endif

%.o: %.c .%.depend
	@echo " [shared] CC $@"
	@$(CC) $(CFLAGS) -o $@ -c $<

.depend: $(OBJS:%.o=%.c)
	@$(CC) $(CFLAGS) -M $^ > .depend

.%.depend: %.c
	@$(CC) $(CFLAGS) -M $< > $@

.defaults.depend .tcode.depend: .%.depend: %.c %.prep
	@$(CC) $(CFLAGS) -M $< > $@

%.prep: %.c
	@$(CC) $(CFLAGS) -E $< > $@

-include $(OBJS:%.o=.%.depend)

