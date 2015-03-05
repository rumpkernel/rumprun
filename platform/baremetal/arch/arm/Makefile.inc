OBJS_BMK+=	arch/arm/stubs.o arch/arm/locore.o
LDSCRIPT=	arch/arm/kern.ldscript

arch/arm/locore.o: arch/arm/locore.S
	${CC} ${CFLAGS} -Iinclude -D_LOCORE -c -o $@ $<
