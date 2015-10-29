ASMS=	arch/arm/integrator/locore.o

SRCS+=	arch/arm/integrator/machdep.o
SRCS+=	arch/arm/integrator/serialcons.o

CPPFLAGS+=	-Iarch/arm

.PHONY: archdirs
archdirs:
        mkdir -p ${RROBJ}/platform/arch/arm/integrator
