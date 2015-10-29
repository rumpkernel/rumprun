ASMS=	arch/i386/locore.S

SRCS+=	arch/i386/machdep.c

SRCS+=	arch/x86/boot.c
SRCS+=	arch/x86/cons.c arch/x86/vgacons.c arch/x86/serialcons.c
SRCS+=	arch/x86/cpu_subr.c
SRCS+=	arch/x86/x86_subr.c
SRCS+=	arch/x86/clock.c
SRCS+=	arch/x86/hypervisor.c

CFLAGS+=	-mno-sse -mno-mmx -march=i686

.PHONY: archdirs
archdirs:
	mkdir -p ${RROBJ}/platform/arch/i386
	mkdir -p ${RROBJ}/platform/arch/x86
