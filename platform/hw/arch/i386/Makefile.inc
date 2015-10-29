ASMS=	arch/i386/locore.o

SRCS+=	arch/i386/machdep.o

SRCS+=	arch/x86/boot.o
SRCS+=	arch/x86/cons.o arch/x86/vgacons.o arch/x86/serialcons.o
SRCS+=	arch/x86/cpu_subr.o
SRCS+=	arch/x86/x86_subr.o
SRCS+=	arch/x86/clock.o
SRCS+=      arch/x86/hypervisor.o

CFLAGS+=	-mno-sse -mno-mmx -march=i686

.PHONY: archdirs
archdirs:
	mkdir -p ${RROBJ}/platform/arch/i386
	mkdir -p ${RROBJ}/platform/arch/x86
