OBJS+=		arch/i386/locore32.o arch/i386/cpu_sched.o arch/i386/machdep.o
LDSCRIPT=	arch/i386/kern.ldscript

arch/i386/locore32.o: arch/i386/locore32.S
	${CC} ${CFLAGS} ${CPPFLAGS} -D_LOCORE -c -o arch/i386/locore32.o $<
