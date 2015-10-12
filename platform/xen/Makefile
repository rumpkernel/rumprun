PLATFORM=xen
PLATFORM_DEFAULT_TESTER=xen

include ../../global.mk
-include config.mk
ifdef BUILDRUMP_TOOLFLAGS
include ${BUILDRUMP_TOOLFLAGS}
endif

MAINOBJ= $(abspath $(OBJ_DIR)/rumprun-xen-$(MACHINE_ARCH).o)

#
# Rumprun-xen Makefile.
# Abandon all hope, ye who enter here:
#   This is in flux while cleanup and separation from the Mini-OS
#   Makefile is being worked out.
#
OBJ_DIR ?= $(CURDIR)/obj

.PHONY: default
default: prepare links commonlibs mini-os $(MAINOBJ) app-tools

include ../Makefile.inc

CPPFLAGS = -isystem xen/include -I. -I../../include
CPPFLAGS+= -no-integrated-cpp -nostdinc

ifeq (${BUILDRR},true)
CPPFLAGS+=     -isystem ${OBJDIR}/dest.stage/include
else
CPPFLAGS+=     -isystem ${DESTDIR}/include
endif

CFLAGS += -fno-builtin ${BUILDRUMP_TOOL_CFLAGS}

# This is semi-duplicated from xen/arch/x86/arch.mk, can we avoid that?
# XXX Which parts of the rumprun source *must* be compiled with these flags?
ifeq ($(MACHINE),i386)
CFLAGS += -march=i686
endif
ifeq ($(MACHINE),x86_64)
CFLAGS += -mno-red-zone -fno-reorder-blocks -fno-asynchronous-unwind-tables
endif

rump-src-y += rumphyper_bio.c

rump-src-y += init.c

# Rump kernel middleware objects to build.
RUMP_OBJS= $(OBJ_DIR)/xen/minios.o $(patsubst %.c,$(OBJ_DIR)/%.o,$(rump-src-y))

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

$(MAINOBJ): $(RUMP_OBJS) commonlibs
	$(CC) -Wl,-r $(CFLAGS) $(LDFLAGS) $(RUMP_OBJS) -nostdlib -o $@

APP_TOOLS_PLATFORM= xen
APP_TOOLS_OBJS_BAKE=	$(MAINOBJ)
APP_TOOLS_LDSCRIPT_BAKE:= $(abspath $(OBJ_DIR)/xen/minios.lds)
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
