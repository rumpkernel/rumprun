LIB=	rumpkern_bmktc

SRCS+=	rump_bmktc.c

RUMPTOP= ${TOPRUMP}

CPPFLAGS+=	-I${RUMPTOP}/librump/rumpkern -I${RUMPTOP}/librump

RUMP_SYM_NORENAME=bmk_platform

.include "${RUMPTOP}/Makefile.rump"
.include <bsd.lib.mk>
.include <bsd.klinks.mk>
