ASMS=	arch/amd64/locore.S arch/amd64/intr.S
SRCS+=	arch/amd64/machdep.c

SRCS+=	arch/x86/boot.c
SRCS+=	arch/x86/cons.c arch/x86/vgacons.c arch/x86/serialcons.c
SRCS+=	arch/x86/cpu_subr.c
SRCS+=	arch/x86/x86_subr.c
SRCS+=	arch/x86/clock.c
SRCS+=	arch/x86/hypervisor.c

CFLAGS+=	-mno-sse -mno-mmx

# squash all interrupts to a single level to
# avoid having to figure out routing
CFLAGS+=	-DBMK_SCREW_INTERRUPT_ROUTING

.PHONY: archdirs
archdirs:
	mkdir -p ${RROBJ}/platform/arch/amd64
	mkdir -p ${RROBJ}/platform/arch/x86
