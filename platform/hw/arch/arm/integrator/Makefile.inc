ASMS=	arch/arm/integrator/locore.S

SRCS+=	arch/arm/integrator/machdep.c
SRCS+=	arch/arm/integrator/serialcons.c

CPPFLAGS+=	-Iarch/arm

.PHONY: archdirs
archdirs:
        mkdir -p ${RROBJ}/platform/arch/arm/integrator
