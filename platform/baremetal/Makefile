include ../../config.mk
include ../../global.mk
-include config.mk
ifdef BUILDRUMP_TOOLFLAGS
include ${BUILDRUMP_TOOLFLAGS}
endif

CPPFLAGS=	-Iinclude -Irump/include -I../../include -nostdinc
CFLAGS+=	${BUILDRUMP_TOOL_CFLAGS}

# Check if we're building for a supported target. For the time being,
# we build x86_64 in 32bit mode, because someone was lazy and did
# not write the 64bit bootstrap.
supported= false
HASPCI= y
# assume we're doing "make clean"
MACHINE?= i386
ifeq (${MACHINE},i386)
supported:= true
endif
ifeq (${MACHINE},arm)
ifdef IWANTARM
supported:= true
HASPCI:= n
endif
endif
ifneq (${supported},true)
$(error only supported target is x86, you have ${MACHINE})
endif

LDFLAGS:= -L$(abspath rump/lib)

ifeq (${CONFIG_SYSPROXY},yes)
LIBS_SYSPROXY=	-lrumpkern_sysproxy
endif

all: app-tools include/bmk/machine rumprun.o tests

OBJS_BMK-y+=		intr.o clock.o kernel.o undefs.o
OBJS_BMK-y+=		rumpuser.o rumpconfig.o
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
LIBS_FS=	-lrumpfs_ffs -lrumpfs_cd9660

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
		${LIBS_FS}					\
		${LIBS_SYSPROXY}				\
		-lrumpdev -lrumpvfs -lrump

COREDIR:=	$(shell pwd)/../../lib/libbmk_core
RUMPUSERDIR:=	$(shell pwd)/../../lib/libbmk_rumpuser
BASEDIR:=	$(shell pwd)/../../lib/librumprun_base

OBJS_BMK+=	init.o
LIBS_USER=	-L${BASEDIR}/baremetal -L${COREDIR}/baremetal -L${RUMPUSERDIR}/baremetal --start-group -lrumprun_base -lpthread -lc --end-group -lbmk_rumpuser -lbmk_core
RUMP_LDLIBS=	--whole-archive ${RUMPKERN_LIB} --no-whole-archive ${LIBS_USER}

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

${BASEDIR}/baremetal/librumprun_base.a:
	( cd ${BASEDIR} \
	    && ${RUMPMAKE} MAKEOBJDIR=baremetal obj \
	    && ${RUMPMAKE} MAKEOBJDIR=baremetal dependall )

${COREDIR}/baremetal/libbmk_core.a:
	( cd ${COREDIR} \
	    && ${RUMPMAKE} MAKEOBJDIR=baremetal obj \
	    && ${RUMPMAKE} MAKEOBJDIR=baremetal dependall )

${RUMPUSERDIR}/baremetal/libbmk_rumpuser.a:
	( cd ${RUMPUSERDIR} \
	    && ${RUMPMAKE} MAKEOBJDIR=baremetal obj \
	    && ${RUMPMAKE} MAKEOBJDIR=baremetal dependall )

rumprun.o: ${OBJS} ${BASEDIR}/baremetal/librumprun_base.a ${COREDIR}/baremetal/libbmk_core.a ${RUMPUSERDIR}/baremetal/libbmk_rumpuser.a
	${CC} -nostdlib ${CFLAGS} -Wl,-r ${OBJS_BMK} -o $@

tests: rumprun.o app-tools
	$(APP_TOOLS_MAKE) -C ../../tests

clean: app-tools_clean
	rm -f ${OBJS} include/bmk/machine
	$(MAKE) -C ../../tests clean
	( cd ${BASEDIR} && ${RUMPMAKE} MAKEOBJDIR=baremetal cleandir )
	( cd ${COREDIR} && ${RUMPMAKE} MAKEOBJDIR=baremetal cleandir )
	( cd ${RUMPUSERDIR} && ${RUMPMAKE} MAKEOBJDIR=baremetal cleandir )

cleandir: clean

cleanrump: clean
	rm -rf rump rumpobj rumptools

distcleanrump: cleanrump
	rm -f config.mk
