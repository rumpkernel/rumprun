PLATFORM=xen
PLATFORM_DEFAULT_TESTER=xen

.PHONY: default
default: prepare mini-os commonlibs rumprun app-tools buildtest

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

include ../Makefile.inc

OBJCOPY=objcopy

CPPFLAGS = -isystem rump/include -isystem xen/include -I. -I../../include
CPPFLAGS+= -nostdinc
CPPFLAGS += -DVIRTIF_BASE=xenif
CPPFLAGS+= -no-integrated-cpp -nostdinc

CFLAGS += -fno-builtin ${BUILDRUMP_TOOL_CFLAGS}

# This is semi-duplicated from xen/arch/x86/arch.mk, can we avoid that?
# XXX Which parts of the rumprun source *must* be compiled with these flags?
ifeq ($(MACHINE),i386)
CFLAGS += -march=i686
endif
ifeq ($(MACHINE),x86_64)
CFLAGS += -mno-red-zone -fno-reorder-blocks -fno-asynchronous-unwind-tables
endif

LDFLAGS=	-L$(abspath rump/lib)

rump-src-y += rumphyper_bio.c
rump-src-y += rumphyper_net.c

rump-src-y += callmain.c

# Rump kernel middleware objects to build.
RUMP_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(rump-src-y))

$(OBJ_DIR)/%.o: %.c $(HDRS) $(EXTRA_DEPS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

.PHONY: prepare
prepare:
	mkdir -p $(OBJ_DIR)/lib

.PHONY: mini-os
mini-os:
	$(MAKE) -C xen OBJ_DIR=$(OBJ_DIR)/xen

links:
	$(MAKE) -C xen links

.PHONY: rumprun
rumprun: $(OBJ_DIR)/rumprun.o

$(OBJ_DIR)/rumprun.o: $(RUMP_OBJS) commonlibs
	$(CC) -Wl,-r $(CFLAGS) $(LDFLAGS) $(RUMP_OBJS) -nostdlib -o $@

APP_TOOLS_TARGETARCH= $(MACHINE)
APP_TOOLS_PLATFORM= xen
APP_TOOLS_HEADOBJ= $(abspath $(OBJ_DIR)/xen/minios.o)
APP_TOOLS_OBJS= $(abspath $(OBJ_DIR)/rumprun.o)
APP_TOOLS_LDSCRIPT:= $(abspath $(OBJ_DIR)/xen/minios.lds)
APP_TOOLS_DIR= $(abspath ../../app-tools)

include $(APP_TOOLS_DIR)/Makefile.app-tools

.PHONY: clean arch_clean app-tools_clean

clean:	app-tools_clean commonclean
	$(MAKE) -C xen OBJ_DIR=$(OBJ_DIR)/xen clean
	rm -f $(OBJ_DIR)/*.o $(OBJ_DIR)/lib/*.o buildtest

cleanrump: clean
	rm -rf rump rumpobj rumptools

distcleanrump: cleanrump
	rm -f config.mk
	$(MAKE) -C ../../tests clean
