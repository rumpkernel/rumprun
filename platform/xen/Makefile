include ../../config.mk
include ../../global.mk
-include config.mk
ifdef BUILDRUMP_TOOLFLAGS
include ${BUILDRUMP_TOOLFLAGS}
endif

#
# Rumprun-xen Makefile.
# Abandon all hope, ye who enter here:
#   This is in flux while cleanup and separation from the Mini-OS
#   Makefile is being worked out.
#
OBJ_DIR ?= $(CURDIR)/obj

OBJCOPY=objcopy

CPPFLAGS = -isystem rump/include -isystem xen/include -I. -I../../include
CPPFLAGS+= -nostdinc
CPPFLAGS += -DVIRTIF_BASE=xenif
CPPFLAGS+= -no-integrated-cpp -nostdinc

CFLAGS += -fno-builtin ${BUILDRUMP_TOOL_CFLAGS}

# This is semi-duplicated from xen/arch/x86/arch.mk, can we avoid that?
TARGET_ARCH := ${MACHINE}
# XXX Which parts of the rumprun source *must* be compiled with these flags?
ifeq ($(TARGET_ARCH),i386)
CFLAGS += -march=i686
endif
ifeq ($(TARGET_ARCH),x86_64)
CFLAGS += -mno-red-zone -fno-reorder-blocks -fno-asynchronous-unwind-tables
endif

COREDIR:=       $(shell pwd)/../../lib/libbmk_core
RUMPUSERDIR:=	$(shell pwd)/../../lib/libbmk_rumpuser
BASEDIR:=       $(shell pwd)/../../lib/librumprun_base

ifeq (${CONFIG_SYSPROXY},yes)
RUMP_LIBS_SYSPROXY = -lrumpkern_sysproxy
endif

RUMP_LIBS_PCI = -lrumpdev_pci -lrumpdev_pci_if_wm -lrumpdev_miiphy
RUMP_LIBS_FS = -lrumpfs_ffs -lrumpfs_cd9660 -lrumpfs_tmpfs
RUMP_LIBS_FS+= -lrumpdev_disk -lrumpdev -lrumpvfs
RUMP_LIBS_NET = -lrumpnet_config -lrumpdev_bpf -lrumpnet_xenif -lrumpnet_netinet
RUMP_LIBS_NET+= -lrumpnet_local -lrumpnet_net -lrumpxen_xendev -lrumpnet

# Define some default flags for linking.
RUMP_LDLIBS = --whole-archive ${RUMP_LIBS_FS} ${RUMP_LIBS_NET} ${RUMP_LIBS_PCI} ${RUMP_LIBS_SYSPROXY} -lrumpxen_tc -lrump --no-whole-archive
RUMP_LDLIBS := ${RUMP_LDLIBS} -L${BASEDIR}/xen -L${COREDIR}/xen -L${RUMPUSERDIR}/xen --start-group -lrumprun_base -lpthread -lc --end-group -lbmk_rumpuser -lbmk_core

LDFLAGS := -L$(abspath rump/lib)

rump-src-y += rumphyper_bio.c
rump-src-y += rumphyper_net.c
rump-src-y += rumphyper_pci.c

rump-src-y += callmain.c
rump-src-y += rumpconfig.c

# Rump kernel middleware objects to build.
RUMP_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(rump-src-y))

$(OBJ_DIR)/%.o: %.c $(HDRS) $(EXTRA_DEPS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

.PHONY: default
default: prepare mini-os commonlibs rumprun app-tools buildtest

.PHONY: prepare
prepare:
	mkdir -p $(OBJ_DIR)/lib

.PHONY: mini-os
mini-os:
	$(MAKE) -C xen OBJ_DIR=$(OBJ_DIR)/xen

.PHONY: ${BASEDIR}/xen/librumprun_base.a
${BASEDIR}/xen/librumprun_base.a:
	( cd ${BASEDIR} \
	    && ${RUMPMAKE} MAKEOBJDIR=xen obj \
	    && ${RUMPMAKE} MAKEOBJDIR=xen dependall )

.PHONY: ${COREDIR}/xen/libbmk_core.a
${COREDIR}/xen/libbmk_core.a:
	( cd ${COREDIR} \
	    && ${RUMPMAKE} MAKEOBJDIR=xen obj \
	    && ${RUMPMAKE} MAKEOBJDIR=xen dependall )

.PHONY: ${RUMPUSERDIR}/xen/libbmk_rumpuser.a
${RUMPUSERDIR}/xen/libbmk_rumpuser.a:
	( cd ${RUMPUSERDIR} \
	    && ${RUMPMAKE} MAKEOBJDIR=xen obj \
	    && ${RUMPMAKE} MAKEOBJDIR=xen dependall )

.PHONY: rumprun
rumprun: $(OBJ_DIR)/rumprun.o

$(OBJ_DIR)/rumprun.o: $(RUMP_OBJS) ${BASEDIR}/xen/librumprun_base.a ${COREDIR}/xen/libbmk_core.a ${RUMPUSERDIR}/xen/libbmk_rumpuser.a
	$(CC) -Wl,-r $(CFLAGS) $(LDFLAGS) $(RUMP_OBJS) -nostdlib -o $@

commonlibs: ${BASEDIR}/xen/librumprun_base.a ${COREDIR}/xen/libbmk_core.a ${RUMPUSERDIR}/xen/libbmk_rumpuser.a

buildtest: ../../tests/hello/hello.c $(OBJ_DIR)/rumprun.o commonlibs app-tools
	$(APP_TOOLS_CC) -o $@ $< -lrumprun_tester

APP_TOOLS_TARGETARCH= $(TARGET_ARCH)
APP_TOOLS_PLATFORM= xen
APP_TOOLS_HEADOBJ= $(abspath $(OBJ_DIR)/xen/minios.o)
APP_TOOLS_OBJS= $(abspath $(OBJ_DIR)/rumprun.o)
APP_TOOLS_LDSCRIPT:= $(abspath $(OBJ_DIR)/xen/minios.lds)
APP_TOOLS_DIR= $(abspath ../../app-tools)

include $(APP_TOOLS_DIR)/Makefile.app-tools

.PHONY: tests
tests:
	RUMPRUN_PLATFORM=xen ../../tests/buildtests.sh
	../../tests/runtests.sh xen

.PHONY: clean arch_clean app-tools_clean

clean:	app-tools_clean
	$(MAKE) -C xen OBJ_DIR=$(OBJ_DIR)/xen clean
	rm -f $(OBJ_DIR)/*.o $(OBJ_DIR)/lib/*.o buildtest
	( cd ${BASEDIR} && ${RUMPMAKE} MAKEOBJDIR=xen cleandir )
	( cd ${COREDIR} && ${RUMPMAKE} MAKEOBJDIR=xen cleandir )
	( cd ${RUMPUSERDIR} && ${RUMPMAKE} MAKEOBJDIR=xen cleandir )

cleanrump: clean
	rm -rf rump rumpobj rumptools

distcleanrump: cleanrump
	rm -f config.mk
	$(MAKE) -C ../../tests clean
