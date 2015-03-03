#
# x86 architecture specific makefiles.
# It's is used for x86_32, x86_32y and x86_64
#

MINI-OS_ROOT=../..
export MINI-OS_ROOT

include $(MINI-OS_ROOT)/Config.mk

# include arch.mk has to be before mini-os.mk!

include arch.mk
include $(MINI-OS_ROOT)/minios.mk

# TODO: Import rump CPPFLAGS / include directories from top-level Makefile
CFLAGS+= -isystem $(MINI-OS_ROOT)/../rump/include -nostdinc

# Sources here are all *.c *.S without $(XEN_TARGET_ARCH).S
# This is handled in $(HEAD_ARCH_OBJ)
ARCH_SRCS := $(wildcard *.c)

# The objects built from the sources.
ARCH_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(ARCH_SRCS))

all: $(OBJ_DIR)/$(ARCH_LIB)

# $(HEAD_ARCH_OBJ) is only build here, needed on linking
# in ../../Makefile.
$(OBJ_DIR)/$(ARCH_LIB): $(ARCH_OBJS) $(OBJ_DIR)/$(HEAD_ARCH_OBJ)
	$(AR) rv $(OBJ_DIR)/$(ARCH_LIB) $(ARCH_OBJS)

clean:
	rm -f $(OBJ_DIR)/$(ARCH_LIB) $(ARCH_OBJS) $(OBJ_DIR)/$(HEAD_ARCH_OBJ)

