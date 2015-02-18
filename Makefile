#
# Rumprun-xen Makefile.
# Abandon all hope, ye who enter here:
#   This is in flux while cleanup and separation from the Mini-OS
#   Makefile is being worked out.
#
OBJ_DIR ?= $(CURDIR)/obj

CONFIG_SYSPROXY ?=	no
CONFIG_CXX ?=		no

OBJCOPY=objcopy

CPPFLAGS = -isystem rump/include -isystem xen/include -I. -nostdinc
CPPFLAGS += -DVIRTIF_BASE=xenif

CFLAGS = -Wall -g
CFLAGS += -fno-builtin -no-integrated-cpp -fno-stack-protector

# This is semi-duplicated from xen/arch/x86/arch.mk, can we avoid that?
TARGET_ARCH := $(shell uname -m | sed -e s/i.86/i386/)
# XXX Which parts of the rumprun source *must* be compiled with these flags?
ifeq ($(TARGET_ARCH),i386)
CFLAGS += -m32 -march=i686
endif
ifeq ($(TARGET_ARCH),x86_64)
CFLAGS += -m64 -mno-red-zone -fno-reorder-blocks -fno-asynchronous-unwind-tables
endif

RUMP_LIBS_PCI = -lrumpdev_pci -lrumpdev_pci_if_wm -lrumpdev_miiphy
RUMP_LIBS_FS = -lrumpfs_ffs -lrumpfs_cd9660 -lrumpdev_disk -lrumpdev -lrumpvfs
RUMP_LIBS_NET = -lrumpnet_config -lrumpdev_bpf -lrumpnet_xenif -lrumpnet_netinet
RUMP_LIBS_NET+= -lrumpnet_net -lrumpxen_xendev -lrumpnet

# Define some default flags for linking.
RUMP_LDLIBS = --whole-archive ${RUMP_LIBS_FS} ${RUMP_LIBS_NET} ${RUMP_LIBS_PCI} ${RUMP_LIBS_SYSPROXY} -lrump --no-whole-archive
RUMP_LDLIBS := ${RUMP_LDLIBS} -lpthread -lc

LDFLAGS := -L$(abspath rump/lib)

rump-src-y += lib/__errno.c
rump-src-y += lib/emul.c
rump-src-y += lib/libc_stubs.c
rump-src-y += lib/memalloc.c
rump-src-y += lib/_lwp.c

rump-src-y += rumphyper_base.c
rump-src-y += rumphyper_net.c
rump-src-y += rumphyper_pci.c
rump-src-y += rumphyper_synch.c
rump-src-y += rumphyper_stubs.c

rump-src-y += callmain.c
rump-src-y += netbsd_init.c
rump-src-y += rumpconfig.c

ifeq (${CONFIG_SYSPROXY},y)
rump-src-${CONFIG_SYSPROXY} += sysproxy.c
CPPFLAGS+= -DRUMP_SYSPROXY
RUMP_LIBS_SYSPROXY = -lrumpkern_sysproxy
endif

# Rump kernel middleware objects to build.
RUMP_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(rump-src-y))

$(OBJ_DIR)/%.o: %.c $(HDRS) $(EXTRA_DEPS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

.PHONY: default
default: prepare mini-os rumprun app-tools tests rump-kernel img/test.ffs

.PHONY: prepare
prepare:
	mkdir -p $(OBJ_DIR)/lib

.PHONY: mini-os
mini-os:
	$(MAKE) -C xen OBJ_DIR=$(OBJ_DIR)/xen

.PHONY: rumprun
rumprun: $(OBJ_DIR)/rumprun.o

$(OBJ_DIR)/rumprun.o: $(RUMP_OBJS)
	$(LD) -r $(LDFLAGS) $(RUMP_OBJS) -o $@

APP_TOOLS_PLATFORM= xen
include Makefile.app-tools

# New demos each have their own Makefile under tests/ and are built using
# app-tools.
.PHONY: tests
tests:
	$(APP_TOOLS_MAKE) -C tests

# This is the "old" rumprun-xen demo, migrated to app-tools. Do not remove this
# demo from the build without prior coordination, Xen oss-test CI relies on it.
.PHONY: httpd
httpd:
	$(APP_TOOLS_MAKE) -C httpd -f Makefile.boot

STDTESTS=tests/libstdtests/rumpkern_demo.c tests/libstdtests/pthread_test.c \
	 tests/libstdtests/tls_test.c
rump-kernel: ${STDTESTS} httpd
	app-tools/rumprun-xen-cc -o $@ ${STDTESTS} httpd/*.o

img/test.ffs:
	cp img/test_clean.ffs img/test.ffs

.PHONY: clean arch_clean app-tools_clean

clean:	app-tools_clean
	$(MAKE) -C xen OBJ_DIR=$(OBJ_DIR)/xen clean
	rm -f $(OBJ_DIR)/*.o $(OBJ_DIR)/lib/*.o rump-kernel
	$(MAKE) -C httpd -f Makefile.boot clean
	$(MAKE) -C tests clean

cleanrump: clean
	rm -rf rump rumpobj rumptools

distcleanrump: cleanrump

