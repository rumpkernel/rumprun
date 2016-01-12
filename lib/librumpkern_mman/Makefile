.include <bsd.own.mk>

LIB=	rumpkern_mman

SRCS+=	sys_mman.c mman_component.c

RUMPTOP= ${TOPRUMP}

CPPFLAGS+= -I${RUMPTOP}/librump/rumpkern

.include "${RUMPTOP}/Makefile.rump"
.include <bsd.lib.mk>
.include <bsd.klinks.mk>
