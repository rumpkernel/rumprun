LIB=		rumprun_core
LIBISPRIVATE=	# defined

SRCS=		init.c bmk_string.c memalloc.c subr_prf.c

SRCS+=		rumpuser_stubs.c

CPPFLAGS+=	-I${.CURDIR}/../../include

.include "${.CURDIR}/arch/${MACHINE}/Makefile.inc"

.include <bsd.lib.mk>
