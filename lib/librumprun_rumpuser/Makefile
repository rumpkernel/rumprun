LIB=		rumprun_rumpuser
LIBISPRIVATE=	# defined

SRCS=		rumpuser_base.c
SRCS+=		rumpuser_clock.c
SRCS+=		rumpuser_mem.c
SRCS+=		rumpuser_synch.c

SRCS+=		rumpuser_stubs.c

CPPFLAGS+=	-I${.CURDIR}/../../include

.include <bsd.lib.mk>
