OBJS_BMK+=	arch/arm/integrator/locore.o
OBJS_BMK+=	arch/arm/integrator/machdep.o
OBJS_BMK+=	arch/arm/integrator/serialcons.o

CPPFLAGS+=	-Iarch/arm

arch/arm/integrator/locore.o: arch/arm/integrator/locore.S
	${CC} ${CFLAGS} ${CPPFLAGS} -D_LOCORE -c -o $@ $<
