.include <bsd.own.mk>

RUMPTOP= ${TOPRUMP}

LIB=	rumprun_unwind

CPPFLAGS+= -isystem ${DESTDIR}/usr/include/c++

.include "${RUMPTOP}/../lib/libunwind/Makefile.inc"

.include <bsd.lib.mk>
