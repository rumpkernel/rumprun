include ../../../global.mk
-include ../config.mk

# Common Makefile for mini-os.
#
# Every architecture directory below mini-os/arch has to have a
# Makefile and a arch.mk.
#

MINI-OS_ROOT=$(CURDIR)
export MINI-OS_ROOT

include Config.mk

# Configuration defaults
CONFIG_PCI ?= y

# Export config items as compiler directives
flags-$(CONFIG_PCI) += -DCONFIG_PCI

DEF_CFLAGS += $(flags-y)

# Include common mini-os makerules.
include minios.mk

# TODO: Import rump CPPFLAGS / include directories from top-level Makefile
CFLAGS += -I.. -I../../../include
CFLAGS += -I$(MINI-OS_ROOT)
CFLAGS += ${BUILDRUMP_TOOL_CFLAGS}

CFLAGS += -nostdinc -no-integrated-cpp

LDARCHLIB := -l$(ARCH_LIB_NAME)
LDSCRIPT := $(TARGET_ARCH_DIR)/minios-$(XEN_TARGET_ARCH).lds
LDFLAGS_FINAL := -T $(LDSCRIPT)

LDFLAGS := -L$(abspath $(OBJ_DIR)/$(TARGET_ARCH_DIR))

# Prefixes for global API names. All other symbols in mini-os are localised
# before linking with rumprun applications.
GLOBAL_PREFIXES := _minios_ minios_ HYPERVISOR_ blkfront_ netfront_ pcifront_ xenbus_ gntmap_ bmk_
GLOBAL_PREFIXES := $(patsubst %,-G %*, $(GLOBAL_PREFIXES))

# Subdirectories common to mini-os
SUBDIRS := console xenbus

src-y += blkfront.c
src-y += events.c
src-y += gntmap.c
src-y += gnttab.c
src-y += hypervisor.c
src-y += kernel.c
src-y += mm.c
src-y += netfront.c
src-$(CONFIG_PCI) += pcifront.c
src-y += xenbus/xenbus.c
src-y += console/console.c
src-y += console/xencons_ring.c
src-y += console/xenbus.c

# The common mini-os objects to build.
OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(src-y))

.PHONY: default
default: objs $(OBJ_DIR)/minios.o $(OBJ_DIR)/minios.lds

objs:
	mkdir -p $(OBJ_DIR)/$(TARGET_ARCH_DIR)
	mkdir -p $(OBJ_DIR)/console $(OBJ_DIR)/xenbus

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
	[ -e ../../../include/bmk-pcpu ] || ln -sf ../platform/xen/xen/include/mini-os/machine ../../../include/bmk-pcpu

.PHONY: arch_lib
arch_lib:
	$(MAKE) --directory=$(TARGET_ARCH_DIR) OBJ_DIR=$(OBJ_DIR)/$(TARGET_ARCH_DIR) || exit 1;

# XXX: Note that HEAD_OBJ is linked into the final minios.o
# This is so that we don't have to hunt for it in the rumprun build of app-tools.
$(OBJ_DIR)/minios.o: links $(OBJS) arch_lib
	$(CC) -Wl,-r $(CFLAGS) $(LDFLAGS) $(HEAD_OBJ) $(OBJS) -Wl,--whole-archive $(LDARCHLIB) -Wl,--no-whole-archive $(LDLIBS) -nostdlib -o $@
	$(OBJCOPY) -w $(GLOBAL_PREFIXES) -G _start $@ $@

$(OBJ_DIR)/minios.lds: $(LDSCRIPT)
	cp $(LDSCRIPT) $@

.PHONY: clean arch_clean

arch_clean:
	$(MAKE) --directory=$(TARGET_ARCH_DIR) OBJ_DIR=$(OBJ_DIR)/$(TARGET_ARCH_DIR) clean || exit 1;

clean:	arch_clean
	for dir in $(addprefix $(OBJ_DIR)/,$(SUBDIRS)); do \
		rm -f $$dir/*.o; \
	done
	rm -f $(OBJ_DIR)/*.o *~ $(OBJ_DIR)/core
	rm -f $(OBJ_DIR)/minios.lds
	rm -f include/xen include/mini-os/machine
