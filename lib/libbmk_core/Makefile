LIB=		bmk_core
LIBISPRIVATE=	# defined

SRCS=		init.c bmk_string.c jsmn.c memalloc.c sched.c subr_prf.c

# kernel-level source code
CFLAGS+=	-fno-stack-protector

CPPFLAGS+=	-I${.CURDIR}/../../include

.include "${.CURDIR}/arch/${MACHINE}/Makefile.inc"

.include <bsd.lib.mk>
