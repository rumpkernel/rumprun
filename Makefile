THEBIN= rk.bin

# Has to be an i386 target compiler.  Don't care about much else.
# Easiest way for me to get an i386 target compiler was to let
# NetBSD's build.sh create it for me.  YMMV.
TOOLDIR=/home/pooka/src/nbsrc/obj.i386/tooldir.Linux-3.13.0-32-generic-x86_64/bin
AS= ${TOOLDIR}/i486-netbsdelf-as
CC= ${TOOLDIR}/i486--netbsdelf-gcc
STRIP= ${TOOLDIR}/i486--netbsdelf-strip

# Naturally this has to be an installation compiled for i386
RUMPKERNDIR= /home/pooka/src/buildrump.sh/rump

COMPILER_RT= librt/divdi3.o librt/udivmoddi4.o librt/udivsi3.o librt/udivdi3.o librt/moddi3.o librt/umoddi3.o

CFLAGS=		-std=gnu99 -g -Wall -Werror
CPPFLAGS=	-Iinclude -I${RUMPKERNDIR}/include

OBJS=		intr.o kernel.o undefs.o memalloc.o sched.o subr.o
OBJS+=		rumpuser.o rumpfiber.o rumppci.o
#OBJS+=		app.o
OBJS+=		arch/i386/cpu_sched.o arch/i386/machdep.o
LDSCRIPT=	arch/i386/kern.ldscript

LIBS_PCINET=	-lrumpdev_bpf -lrumpdev_pci_if_wm -lrumpdev_miiphy -lrumpdev_pci
LIBS_NETINET=	-lrumpnet_config -lrumpnet_netinet -lrumpnet_net -lrumpnet

all: ${THEBIN}

${THEBIN}: ${THEBIN}.gdb
	${STRIP} -g -o $@ $<

${THEBIN}.gdb: locore32.o ${OBJS} ${COMPILER_RT} ${LDSCRIPT}
	${CC} -ffreestanding -nostdlib -o $@ -T ${LDSCRIPT} locore32.o ${OBJS} -L${RUMPKERNDIR}/lib -Wl,--whole-archive ${LIBS_PCINET} ${LIBS_NETINET} -lrumpdev -lrumpvfs -lrump -Wl,--no-whole-archive ${COMPILER_RT}

locore32.o: arch/i386/locore32.S
	${CC} ${CFLAGS} -Iinclude -D_LOCORE -c -o locore32.o $<

clean:
	rm -f locore32.o ${OBJS} ${COMPILER_RT} ${THEBIN} ${THEBIN}.gdb
