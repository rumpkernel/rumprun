# Common Makefile for mini-os.
#
# Every architecture directory below mini-os/arch has to have a
# Makefile and a arch.mk.
#

export XEN_ROOT = $(CURDIR)/../..
include $(XEN_ROOT)/Config.mk
OBJ_DIR ?= $(CURDIR)

ifeq ($(MINIOS_CONFIG),)
include Config.mk
else
EXTRA_DEPS += $(MINIOS_CONFIG)
include $(MINIOS_CONFIG)
endif

# Configuration defaults
CONFIG_START_NETWORK ?= y
CONFIG_SPARSE_BSS ?= y
CONFIG_PCIFRONT ?= n
CONFIG_BLKFRONT ?= y
CONFIG_NETFRONT ?= y
CONFIG_FBFRONT ?= n
CONFIG_KBDFRONT ?= n
CONFIG_CONSFRONT ?= y
CONFIG_XENBUS ?= y

# Export config items as compiler directives
flags-$(CONFIG_START_NETWORK) += -DCONFIG_START_NETWORK
flags-$(CONFIG_SPARSE_BSS) += -DCONFIG_SPARSE_BSS
flags-$(CONFIG_QEMU_XS_ARGS) += -DCONFIG_QEMU_XS_ARGS
flags-$(CONFIG_PCIFRONT) += -DCONFIG_PCIFRONT
flags-$(CONFIG_BLKFRONT) += -DCONFIG_BLKFRONT
flags-$(CONFIG_NETFRONT) += -DCONFIG_NETFRONT
flags-$(CONFIG_KBDFRONT) += -DCONFIG_KBDFRONT
flags-$(CONFIG_FBFRONT) += -DCONFIG_FBFRONT
flags-$(CONFIG_CONSFRONT) += -DCONFIG_CONSFRONT
flags-$(CONFIG_XENBUS) += -DCONFIG_XENBUS

DEF_CFLAGS += $(flags-y)

# Include common mini-os makerules.
include minios.mk

CFLAGS += -Irump/include -nostdinc
CFLAGS += -DVIRTIF_BASE=xenif -I.

LIBS_FS = -lrumpfs_ffs -lrumpdev_disk -lrumpdev -lrumpvfs
LIBS_NET = -lrumpnet_config -lrumpdev_bpf -lrumpnet_xenif -lrumpnet_netinet
LIBS_NET+= -lrumpnet_net -lrumpnet

# Define some default flags for linking.
LDLIBS_FS = --whole-archive ${LIBS_FS} ${LIBS_NET} -lrump --no-whole-archive
LDLIBS = -Lrump/lib ${LDLIBS_FS} -lc

APP_LDLIBS := 
LDARCHLIB := -L$(OBJ_DIR)/$(TARGET_ARCH_DIR) -l$(ARCH_LIB_NAME)
LDFLAGS_FINAL := -T $(TARGET_ARCH_DIR)/minios-$(XEN_TARGET_ARCH).lds

# Prefix for global API names. All other symbols are localised before
# linking with EXTRA_OBJS.
GLOBAL_PREFIX := xenos_
EXTRA_OBJS =

TARGET := rump-kernel

# Subdirectories common to mini-os
SUBDIRS := lib xenbus console

src-$(CONFIG_BLKFRONT) += blkfront.c
src-y += events.c
src-$(CONFIG_FBFRONT) += fbfront.c
src-y += gntmap.c
src-y += gnttab.c
src-y += hypervisor.c
src-y += kernel.c
src-y += libc_stubs.c
src-y += mm.c
src-$(CONFIG_NETFRONT) += netfront.c
src-$(CONFIG_PCIFRONT) += pcifront.c
src-y += sched.c

src-y += lib/__errno.c
src-y += lib/ctype.c
#src-y += lib/math.c
src-y += lib/memalloc.c
src-y += lib/printf.c
src-y += lib/stack_chk_fail.c
src-y += lib/string.c
src-y += lib/strtoul.c

src-y += rumphyper_base.c
src-y += rumphyper_net.c
src-y += rumphyper_synch.c
src-y += rumphyper_stubs.c

src-y += rumpkern_demo.c

src-$(CONFIG_XENBUS) += xenbus/xenbus.c

src-y += console/console.c
src-y += console/xencons_ring.c
src-$(CONFIG_CONSFRONT) += console/xenbus.c

# The common mini-os objects to build.
APP_OBJS :=
OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(src-y))

.PHONY: default
default: $(OBJ_DIR)/$(TARGET)

# Create special architecture specific links. The function arch_links
# has to be defined in arch.mk (see include above).
ifneq ($(ARCH_LINKS),)
$(ARCH_LINKS):
	$(arch_links)
endif

.PHONY: links
links: $(ARCH_LINKS)
	[ -e include/xen ] || ln -sf ../../../xen/include/public include/xen
	[ -e include/mini-os/machine ] || ln -sf $(TARGET_ARCH_FAM) include/mini-os/machine

.PHONY: arch_lib
arch_lib:
	$(MAKE) --directory=$(TARGET_ARCH_DIR) OBJ_DIR=$(OBJ_DIR)/$(TARGET_ARCH_DIR) || exit 1;

$(OBJ_DIR)/$(TARGET)_app.o: $(APP_OBJS) app.lds
	$(LD) -r -d $(LDFLAGS) -\( $^ -\) $(APP_LDLIBS) --undefined main -o $@

ifneq ($(APP_OBJS),)
APP_O=$(OBJ_DIR)/$(TARGET)_app.o 
endif

$(OBJ_DIR)/$(TARGET): links $(OBJS) $(APP_O) arch_lib
	$(LD) -r $(LDFLAGS) $(HEAD_OBJ) $(APP_O) $(OBJS) $(LDARCHLIB) $(LDLIBS) -o $@.o
	$(OBJCOPY) -w -G $(GLOBAL_PREFIX)* -G _start $@.o $@.o
	$(LD) $(LDFLAGS) $(LDFLAGS_FINAL) $@.o $(EXTRA_OBJS) -o $@
	#gzip -f -9 -c $@ >$@.gz

.PHONY: clean arch_clean

arch_clean:
	$(MAKE) --directory=$(TARGET_ARCH_DIR) OBJ_DIR=$(OBJ_DIR)/$(TARGET_ARCH_DIR) clean || exit 1;

clean:	arch_clean
	for dir in $(addprefix $(OBJ_DIR)/,$(SUBDIRS)); do \
		rm -f $$dir/*.o; \
	done
	rm -f $(OBJ_DIR)/*.o *~ $(OBJ_DIR)/core $(OBJ_DIR)/$(TARGET).elf $(OBJ_DIR)/$(TARGET).raw $(OBJ_DIR)/$(TARGET) $(OBJ_DIR)/$(TARGET).gz
	rm -f $(OBJ_DIR)/include/xen $(OBJ_DIR)/include/mini-os/machine
	rm -f tags TAGS


define all_sources
     ( find . -follow -name SCCS -prune -o -name '*.[chS]' -print )
endef

.PHONY: cscope
cscope:
	$(all_sources) > cscope.files
	cscope -k -b -q
    
.PHONY: tags
tags:
	$(all_sources) | xargs ctags

.PHONY: TAGS
TAGS:
	$(all_sources) | xargs etags

