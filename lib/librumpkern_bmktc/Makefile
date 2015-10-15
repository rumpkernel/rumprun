.include <bsd.own.mk>

LIB=	rumpkern_bmktc

SRCS+=	rump_bmktc.c

RUMPTOP= ${TOPRUMP}

RUMPCOMP_USER_SRCS=	bmktc_user.c
RUMPCOMP_USER_CPPFLAGS+=-I${.CURDIR}/../../include

.undef RUMPKERN_ONLY

.include "${RUMPTOP}/Makefile.rump"
.include <bsd.lib.mk>
.include <bsd.klinks.mk>
