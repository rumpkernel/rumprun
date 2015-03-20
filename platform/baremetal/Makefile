include ../../config.mk
include ../../global.mk
-include config.mk

THEBIN= rk.bin
THEISO=	$(basename ${THEBIN}).iso

# answer "yes" if you have built "userspace" (i.e. you've run buildme.sh)
#
# Note, RUMPRUN_PRESENT=no will work only on x86.  On e.g. ARM, we depend
# on libc to provide toolchain helpers.  They could probably be provided
# similar to how we provide librt for x86, but not sure it's worth
# the bother ...
RUMPRUN_PRESENT?= yes

CPPFLAGS=	-Iinclude -Irump/include -I../../include -nostdinc
STRIP?=		strip

MACHINE:= $(shell ${CC} -dumpmachine | sed 's/i.86/i386/;s/-.*//;')

# Check if we're building for a supported target. For the time being,
# we build x86_64 in 32bit mode, because someone was lazy and did
# not write the 64bit bootstrap.
supported= false
HASPCI= y
ifeq (${MACHINE},i386)
supported:= true
endif
ifeq (${MACHINE},arm)
ifdef IWANTARM
supported:= true
HASPCI:= n
endif
endif
ifeq (${MACHINE},x86_64)
  supported:= true
  MACHINE:=i386
  # XXX
  CFLAGS+=-m32
  CFLAGS_PLATFORM=-m32
  LDFLAGS_PLATFORM=-m32
endif
ifneq (${supported},true)
$(error only supported target is x86, you have ${MACHINE})
endif

LDFLAGS:= -L$(abspath rump/lib)

ifeq (${CONFIG_SYSPROXY},yes)
LIBS_SYSPROXY=	-lrumpkern_sysproxy
endif

all: app-tools include/bmk/machine rumprun.o tests

OBJS_BMK-y+=		intr.o kernel.o undefs.o memalloc.o sched.o subr.o
OBJS_BMK-y+=		rumpuser.o rumpfiber.o rumpconfig.o
OBJS_BMK-${HASPCI}+=	rumppci.o

OBJS_BMK+= ${OBJS_BMK-y}

include arch/${MACHINE}/Makefile.inc

ifeq (${HASPCI},y)
LIBS_VIO=	-lrumpdev_pci_virtio
LIBS_VIO_NET=	-lrumpdev_virtio_if_vioif
LIBS_VIO_LD=	-lrumpdev_disk -lrumpdev_virtio_ld
LIBS_VIO_RND=	-lrumpdev_virtio_viornd
LIBS_PCI_NET=	-lrumpdev_pci_if_wm -lrumpdev_miiphy
#LIBS_AUDIO=	-lrumpdev_audio
#LIBS_AUDIO_EAP=-lrumpdev_pci_eap
#LIBS_AUDIO_HDA=-lrumpdev_pci_hdaudio -lrumpdev_hdaudio_hdafg
LIBS_PCI=	-lrumpdev_pci
endif
LIBS_NETINET=	-lrumpnet_config -lrumpnet_netinet -lrumpnet_net -lrumpnet
LIBS_NETBPF=	-lrumpdev_bpf
LIBS_NETUNIX=	-lrumpnet_local

RUMPKERN_LIB=	${LIBS_VIO_NET}					\
		${LIBS_VIO_LD}					\
		${LIBS_VIO_RND}					\
		${LIBS_VIO}					\
		${LIBS_AUDIO}					\
		${LIBS_AUDIO_EAP}				\
		${LIBS_AUDIO_HDA}				\
		${LIBS_PCI_NET}					\
		${LIBS_PCI}					\
		${LIBS_NETINET}					\
		${LIBS_NETBPF}					\
		${LIBS_SYSPROXY}				\
		-lrumpdev -lrumpvfs -lrump

ifeq (${RUMPRUN_PRESENT},yes)
  OBJS_BMK+=	libc_errno.o libc_malloc.o
  OBJS_BMK+=	init.o
  CPPFLAGS+=	-DBMK_APP
  LIBS_USER=	-L$(shell pwd)/../../lib/librumprun_base/baremetal --start-group -lrumprun_base -lpthread -lc --end-group
  RUMP_LDLIBS=	--whole-archive ${RUMPKERN_LIB} --no-whole-archive ${LIBS_USER}
else
  COMPILER_RT=	librt/divdi3.o librt/udivmoddi4.o librt/udivsi3.o
  COMPILER_RT+=	librt/udivdi3.o librt/moddi3.o librt/umoddi3.o
endif

OBJS= ${OBJS_BMK} ${OBJS_APP}

.PHONY:	clean cleandir test

APP_TOOLS_TARGETARCH=${MACHINE}
APP_TOOLS_PLATFORM= bmk
APP_TOOLS_HEADOBJ= $(abspath rumprun.o)
APP_TOOLS_OBJS=
APP_TOOLS_LDSCRIPT:= $(abspath ${LDSCRIPT})
APP_TOOLS_DIR:= $(abspath ../../app-tools)

include $(APP_TOOLS_DIR)/Makefile.app-tools

include/bmk/machine:
	ln -s ../arch/${MACHINE} include/bmk/machine

rumprun.o: ${OBJS}
	${CC} -nostdlib ${CFLAGS} -Wl,-r ${OBJS_BMK} -o $@

tests: rumprun.o app-tools
	CFLAGS=$(CFLAGS_PLATFORM) LDFLAGS=$(LDFLAGS_PLATFORM) $(APP_TOOLS_MAKE) -C ../../tests

clean: app-tools_clean
	rm -f ${OBJS} ${COMPILER_RT} include/bmk/machine
	$(MAKE) -C ../../tests clean

cleandir: clean

cleanrump: clean
	rm -rf rump rumpobj rumptools

distcleanrump: cleanrump
	rm -f config.mk
