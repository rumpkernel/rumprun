OBJS_BMK+=	arch/i386/locore.o arch/i386/machdep.o

OBJS_BMK+=	arch/x86/boot.o
OBJS_BMK+=	arch/x86/cons.o arch/x86/vgacons.o arch/x86/serialcons.o
OBJS_BMK+=	arch/x86/cpu_subr.o
OBJS_BMK+=	arch/x86/x86_subr.o
OBJS_BMK+=	arch/x86/clock.o
OBJS_BMK+=      arch/x86/hypervisor.o

LDSCRIPT=	arch/i386/kern.ldscript

CFLAGS+=	-mno-sse -mno-mmx -march=i686

arch/i386/locore.o: arch/i386/locore.S
	${CC} ${CFLAGS} ${CPPFLAGS} -D_LOCORE -c -o arch/i386/locore.o $<
