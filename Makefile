# Common Makefile for mini-os.
#
# Every architecture directory below mini-os/arch has to have a
# Makefile and a arch.mk.
#

MINI-OS_ROOT=$(CURDIR)
export MINI-OS_ROOT

ifeq ($(MINIOS_CONFIG),)
include Config.mk
else
EXTRA_DEPS += $(MINIOS_CONFIG)
include $(MINIOS_CONFIG)
endif

# Configuration defaults
CONFIG_XENBUS ?= y

CONFIG_PCI ?= y

# Export config items as compiler directives
flags-$(CONFIG_XENBUS) += -DCONFIG_XENBUS
flags-$(CONFIG_PCI) += -DCONFIG_PCI

DEF_CFLAGS += $(flags-y)

OBJCOPY=objcopy

# Include common mini-os makerules.
include minios.mk

CFLAGS += -Irump/include -nostdinc
CFLAGS += -DVIRTIF_BASE=xenif -I$(MINI-OS_ROOT)
CFLAGS += -no-integrated-cpp

ifeq ($(CONFIG_PCI),y)
RUMP_LIBS_PCI = -lrumpdev_pci -lrumpdev_pci_if_wm -lrumpdev_miiphy
endif

RUMP_LIBS_FS = -lrumpfs_ffs -lrumpfs_cd9660 -lrumpdev_disk -lrumpdev -lrumpvfs
RUMP_LIBS_NET = -lrumpnet_config -lrumpdev_bpf -lrumpnet_xenif -lrumpnet_netinet
RUMP_LIBS_NET+= -lrumpnet_net -lrumpxen_xendev -lrumpnet

# Define some default flags for linking.
RUMP_LDLIBS = --whole-archive ${RUMP_LIBS_FS} ${RUMP_LIBS_NET} ${RUMP_LIBS_PCI} -lrump --no-whole-archive
RUMP_LDLIBS := ${RUMP_LDLIBS} -lpthread -lc

LDARCHLIB := -l$(ARCH_LIB_NAME)
LDSCRIPT := xen/$(TARGET_ARCH_DIR)/minios-$(XEN_TARGET_ARCH).lds
LDFLAGS_FINAL := -T $(LDSCRIPT)

LDFLAGS := -L$(abspath $(OBJ_DIR)/xen/$(TARGET_ARCH_DIR)) -L$(abspath rump/lib)

# Prefixes for global API names. All other symbols in mini-os are localised
# before linking with rumprun applications.
GLOBAL_PREFIXES := _minios_ minios_ HYPERVISOR_ blkfront_ netfront_ pcifront_ xenbus_
GLOBAL_PREFIXES := $(patsubst %,-G %*, $(GLOBAL_PREFIXES))

# Subdirectories common to mini-os
SUBDIRS := lib xen xen/console xen/xenbus

src-y += xen/blkfront.c
src-y += xen/events.c
src-y += xen/gntmap.c
src-y += xen/gnttab.c
src-y += xen/hypervisor.c
src-y += xen/kernel.c
src-y += xen/mm.c
src-y += xen/netfront.c
src-$(CONFIG_PCI) += xen/pcifront.c
src-y += xen/sched.c
src-$(CONFIG_XENBUS) += xen/xenbus/xenbus.c
src-y += xen/console/console.c
src-y += xen/console/xencons_ring.c
src-y += xen/console/xenbus.c

rump-src-y += lib/__errno.c
rump-src-y += lib/emul.c
rump-src-y += lib/libc_stubs.c
rump-src-y += lib/memalloc.c
rump-src-y += lib/_lwp.c

rump-src-y += rumphyper_base.c
rump-src-y += rumphyper_net.c
rump-src-$(CONFIG_PCI) += rumphyper_pci.c
rump-src-y += rumphyper_synch.c
rump-src-y += rumphyper_stubs.c

rump-src-y += callmain.c
rump-src-y += netbsd_init.c
rump-src-y += rumpconfig.c

# The common mini-os objects to build.
OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(src-y))
# Rump kernel middleware objects to build.
RUMP_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(rump-src-y))

.PHONY: default
default: objs app-tools minios.o rumprun.o tests/hello/hello rump-kernel

objs:
	mkdir -p $(OBJ_DIR)/lib $(OBJ_DIR)/xen/$(TARGET_ARCH_DIR)
	mkdir -p $(OBJ_DIR)/xen/console $(OBJ_DIR)/xen/xenbus

# Create special architecture specific links. The function arch_links
# has to be defined in arch.mk (see include above).
ifneq ($(ARCH_LINKS),)
$(ARCH_LINKS):
	$(arch_links)
endif

.PHONY: links
links: $(ARCH_LINKS)
	[ -e include/xen ] || ln -sf $(XEN_HEADERS) include/xen
	[ -e include/mini-os/machine ] || ln -sf $(TARGET_ARCH_FAM) include/mini-os/machine

.PHONY: arch_lib
arch_lib:
	$(MAKE) --directory=xen/$(TARGET_ARCH_DIR) OBJ_DIR=$(OBJ_DIR)/xen/$(TARGET_ARCH_DIR) || exit 1;

minios.o: links $(OBJS) arch_lib
	$(LD) -r $(LDFLAGS) $(OBJS) $(LDARCHLIB) $(LDLIBS) -o $@
	$(OBJCOPY) -w $(GLOBAL_PREFIXES) -G _start $@ $@

rumprun.o: $(RUMP_OBJS)
	$(LD) -r $(LDFLAGS) $(RUMP_OBJS) -o $@

APP_TOOLS += rumpapp-xen-cc rumpapp-xen-cc.configure specs specs.configure
APP_TOOLS += rumpapp-xen-configure rumpapp-xen-make rumpapp-xen-gmake

.PHONY: app-tools
app-tools: $(addprefix app-tools/, $(APP_TOOLS))

APP_TOOLS_LDLIBS := $(RUMP_LDLIBS)
# XXX: LDARCHLIB isn't really a linker flag, but it needs to
# be always included anyway
APP_TOOLS_LDFLAGS := $(LDFLAGS) $(LDARCHLIB)
APP_TOOLS_OBJS := $(abspath minios.o rumprun.o)

APP_TOOLS_ARCH := $(subst x86_32,i386, \
                  $(subst x86_64,amd64, \
                  $(XEN_TARGET_ARCH)))

APP_TOOLS_CPPFLAGS := $(filter -U%, $(shell \
	rumptools/rumpmake -f rumptools/mk.conf -V '$${CPPFLAGS}'))

app-tools/%: app-tools/%.in Makefile Config.mk
	sed <$< >$@.tmp \
		-e 's#!ARCH!#$(strip $(APP_TOOLS_ARCH))#g;' \
		-e 's#!BASE!#$(abspath .)#g;' \
		-e 's#!APPTOOLS!#$(abspath app-tools)#g;' \
		-e 's#!CPPFLAGS!#$(APP_TOOLS_CPPFLAGS)#g;' \
		-e 's#!OBJS!#$(APP_TOOLS_OBJS)#g;' \
		-e 's#!LDLIBS!#$(APP_TOOLS_LDLIBS)#g;' \
		-e 's#!LDFLAGS!#$(APP_TOOLS_LDFLAGS)#g;' \
		-e 's#!HEAD_OBJ!#$(abspath $(HEAD_OBJ))#g;' \
		-e 's#!LDSCRIPT!#$(abspath $(LDSCRIPT))#g;'
	if test -x $<; then chmod +x $@.tmp; fi
	mv -f $@.tmp $@

app-tools_clean:
	rm -f $(addprefix app-tools/, $(APP_TOOLS))

tests/hello/hello: tests/hello/hello.c app-tools minios.o rumprun.o
	app-tools/rumpapp-xen-cc -o $@ $<

# This is the "old" rumprun-xen demo, migrated to app-tools. Do not remove this
# demo from the build without prior coordination, Xen oss-test CI relies on it.
.PHONY: httpd
httpd:
	app-tools/rumpapp-xen-make -C httpd -f Makefile.boot

rump-kernel: rumpkern_demo.c pthread_test.c httpd
	app-tools/rumpapp-xen-cc -o $@ rumpkern_demo.c pthread_test.c httpd/*.o

.PHONY: clean arch_clean app-tools_clean

arch_clean:
	$(MAKE) --directory=xen/$(TARGET_ARCH_DIR) OBJ_DIR=$(OBJ_DIR)/xen/$(TARGET_ARCH_DIR) clean || exit 1;

clean:	arch_clean app-tools_clean
	for dir in $(addprefix $(OBJ_DIR)/,$(SUBDIRS)); do \
		rm -f $$dir/*.o; \
	done
	rm -f $(OBJ_DIR)/*.o *~ $(OBJ_DIR)/core minios.o rumprun.o rump-kernel
	rm -f include/xen include/mini-os/machine
	rm -f tags TAGS
	rm -f tests/hello/hello
	make -C httpd -f Makefile.boot clean

cleanrump: clean
	rm -rf rump rumpobj rumptools

distcleanrump: cleanrump

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

