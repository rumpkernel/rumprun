THEBIN= rk.bin

# answer "yes" if you have built "userspace"
RUMPRUN_PRESENT?= no

# Has to be an i386 target compiler.  Don't care about much else.
# Easiest way for me to get an i386 target compiler was to let
# NetBSD's build.sh create it for me.  YMMV.
#TOOLDIR=/home/pooka/src/nbsrc/obj.i386/tooldir.Linux-3.13.0-32-generic-x86_64/bin
#AS= ${TOOLDIR}/i486-netbsdelf-as
#CC= ${TOOLDIR}/i486--netbsdelf-gcc
#STRIP= ${TOOLDIR}/i486--netbsdelf-strip

CFLAGS+=	-std=gnu99 -g -Wall -Werror
CPPFLAGS=	-Iinclude -I${RUMPKERNDIR}/include -nostdinc
STRIP?=		strip

MACHINE:= $(shell ${CC} -dumpmachine | sed 's/i.86/i386/;s/-.*//;')

# Check if we're building for a 32bit target.
# XXX: this is here only to help Travis CI.  The resulting binary will
# not run if you do not build it with a real toolchain
supported= false
ifeq (${MACHINE},i386)
supported:= true
endif
ifeq (${MACHINE},x86_64)
  ifeq ($(shell ${CC} ${CFLAGS} ${CPPFLAGS} -E -dM - < /dev/null | grep LP64),)
    supported:= true
  endif
endif
ifneq (${supported},true)
$(error only supported target is 32bit x86)
endif

# Naturally this has to be an installation compiled for i386
RUMPKERNDIR?=	/home/pooka/src/buildrump.sh/rump

OBJS=		intr.o kernel.o undefs.o memalloc.o sched.o subr.o
OBJS+=		rumpuser.o rumpfiber.o rumppci.o
OBJS+=		arch/i386/cpu_sched.o arch/i386/machdep.o
LDSCRIPT=	arch/i386/kern.ldscript

#LIBS_PCINET=	-lrumpdev_bpf -lrumpdev_pci_if_vioif -lrumpdev_miiphy -lrumpdev_pci
LIBS_PCINET=	-lrumpdev_bpf -lrumpdev_pci_if_wm -lrumpdev_miiphy -lrumpdev_pci
LIBS_NETINET=	-lrumpnet_config -lrumpnet_netinet -lrumpnet_net -lrumpnet
LIBS_NETUNIX=	-lrumpnet_local

ifeq (${RUMPRUN_PRESENT},yes)
  OBJS+=	libc_errno.o libc_emul.o
  OBJS+=	app.o
  CPPFLAGS+=	-DRUMPRUN_APP
  LIBS_USER=	-lc
else
  COMPILER_RT=	librt/divdi3.o librt/udivmoddi4.o librt/udivsi3.o
  COMPILER_RT+=	librt/udivdi3.o librt/moddi3.o librt/umoddi3.o
endif

all: ${THEBIN}

${THEBIN}: ${THEBIN}.gdb
	${STRIP} -g -o $@ $<

${THEBIN}.gdb: locore32.o ${OBJS} ${COMPILER_RT} ${LDSCRIPT} Makefile
	${CC} -ffreestanding -nostdlib -o $@ -T ${LDSCRIPT} locore32.o ${OBJS} -L${RUMPKERNDIR}/lib -Wl,--whole-archive ${LIBS_PCINET} ${LIBS_NETINET} -lrumpdev -lrumpvfs -lrump -Wl,--no-whole-archive ${LIBS_USER} ${COMPILER_RT}

locore32.o: arch/i386/locore32.S
	${CC} ${CFLAGS} -Iinclude -D_LOCORE -c -o locore32.o $<

clean:
	rm -f locore32.o ${OBJS} ${COMPILER_RT} ${THEBIN} ${THEBIN}.gdb
