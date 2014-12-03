THEBIN= rk.bin
THEISO=	$(basename ${THEBIN}).iso

# answer "yes" if you have built "userspace" (i.e. you've run buildme.sh)
RUMPRUN_PRESENT?= yes

CFLAGS+=	-std=gnu99 -g -Wall -O2
ifndef NOGCCERROR
CFLAGS+=	-Werror
endif
CPPFLAGS=	-Iinclude -I${RUMPKERNDIR}/include -nostdinc
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
  CFLAGS+=-m32
  LDFLAGS+=-m32
endif
ifneq (${supported},true)
$(error only supported target is x86, you have ${MACHINE})
endif

# Naturally this has to be an installation compiled for $MACHINE
RUMPKERNDIR?=	${BUILDRUMP_SH}/rump

all: include/machine ${THEBIN}

OBJS-y=			intr.o kernel.o undefs.o memalloc.o sched.o subr.o
OBJS-y+=		rumpuser.o rumpfiber.o
OBJS-${CONFIG_PCI}+=	rumppci.o

OBJS= ${OBJS-y}

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

ALLLIBS=	${LIBS_VIO_NET}					\
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
		-lrumpdev -lrumpvfs -lrump

ifeq (${RUMPRUN_PRESENT},yes)
  OBJS+=	libc_errno.o libc_emul.o libc_malloc.o
  OBJS+=	app.o
  CPPFLAGS+=	-DBMK_APP
  LIBS_USER=	-lcrypto -lc
  CFLAGS+=	-Wmissing-prototypes
else
  COMPILER_RT=	librt/divdi3.o librt/udivmoddi4.o librt/udivsi3.o
  COMPILER_RT+=	librt/udivdi3.o librt/moddi3.o librt/umoddi3.o
endif

.PHONY:	clean cleandir

include/machine:
	ln -s arch/${MACHINE} include/machine

${THEBIN}: ${THEBIN}.gdb
	${STRIP} -g -o $@ $<

${THEBIN}.gdb: ${OBJS} ${COMPILER_RT} ${LDSCRIPT} Makefile
	${CC} -ffreestanding -nostdlib -o $@ -T ${LDSCRIPT} ${LDFLAGS} ${OBJS} -L${RUMPKERNDIR}/lib -Wl,--whole-archive ${ALLLIBS} -Wl,--no-whole-archive ${LIBS_USER} ${COMPILER_RT}

iso/boot/grub/grub.cfg:
	mkdir -p iso/boot/grub
	printf "menuentry "rumpkernel" {\n\tmultiboot /boot/${THEBIN}\n}\n" > $@

${THEISO}: ${THEBIN} iso/boot/grub/grub.cfg
	ln -f ${THEBIN} iso/boot/
	grub-mkrescue -o $@ iso

iso: ${THEISO}

clean:
	rm -f ${OBJS} ${COMPILER_RT} ${THEBIN} ${THEBIN}.gdb \
	    iso/boot/${THEBIN} ${THEISO} iso/boot/grub/grub.cfg \
	    include/machine

cleandir: clean
