LIB=		bmk_core
LIBISPRIVATE=	# defined

SRCS=		init.c bmk_string.c memalloc.c pgalloc.c sched.c
SRCS+=		subr_prf.c strtoul.c

# kernel-level source code
CFLAGS+=	-fno-stack-protector

CPPFLAGS+=	-I${.CURDIR}/../../include

.include "${.CURDIR}/arch/${MACHINE_ARCH}/Makefile.inc"

.include <bsd.lib.mk>
