PLATFORM=xen
PLATFORM_DEFAULT_TESTER=xen

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

TARGETS= prepare links mini-os $(MAINOBJ)

LDSCRIPT:= $(abspath $(OBJ_DIR)/xen/minios.lds)

.PHONY: default
default: ${TARGETS}

CPPFLAGS = -isystem xen/include -I. -I../../include
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

$(MAINOBJ): $(RUMP_OBJS) platformlibs
	$(CC) -Wl,-r $(CFLAGS) $(LDFLAGS) $(RUMP_OBJS) -nostdlib -o $@ \
	    -L${COREDIR}/xen -L${RUMPUSERDIR}/xen -Wl,--whole-archive \
	    -lbmk_rumpuser -lbmk_core -Wl,--no-whole-archive

.PHONY: clean arch_clean

clean:	commonclean
	$(MAKE) -C xen OBJ_DIR=$(OBJ_DIR)/xen clean
	rm -f $(OBJ_DIR)/*.o $(OBJ_DIR)/lib/*.o buildtest $(MAINOBJ)

cleanrump: clean
	rm -rf rump rumpobj rumptools

distcleanrump: cleanrump
	rm -f config.mk
	$(MAKE) -C ../../tests clean
