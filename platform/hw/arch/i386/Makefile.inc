OBJS_BMK+=	arch/i386/locore32.o arch/i386/machdep.o arch/i386/boot.o

OBJS_BMK+=	arch/x86/vgacons.o
OBJS_BMK+=	arch/x86/x86_subr.o

LDSCRIPT=	arch/i386/kern.ldscript

CFLAGS+=	-mno-sse -mno-mmx -march=i686

arch/i386/locore32.o: arch/i386/locore32.S
	${CC} ${CFLAGS} ${CPPFLAGS} -D_LOCORE -c -o arch/i386/locore32.o $<
