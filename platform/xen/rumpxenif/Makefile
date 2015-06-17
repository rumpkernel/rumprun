.include <bsd.own.mk>

LIB=	rumpnet_xenif

SRCS=	if_virt.c
SRCS+=	xenif_component.c

RUMPTOP= ${TOPRUMP}

IFBASE=		-DVIRTIF_BASE=xenif

CPPFLAGS+=	-I${RUMPTOP}/librump/rumpkern -I${RUMPTOP}/librump/rumpnet
CPPFLAGS+=	-I${.CURDIR}
CPPFLAGS+=	${IFBASE}

RUMPCOMP_USER_SRCS=	 rumphyper_net.c
RUMPCOMP_USER_CPPFLAGS+= -I${.CURDIR}/..
RUMPCOMP_USER_CPPFLAGS+= -I${.CURDIR}/../xen/include
RUMPCOMP_USER_CPPFLAGS+= -I${.CURDIR}/../../../include
RUMPCOMP_USER_CPPFLAGS+= ${IFBASE}

# XXX
.undef RUMPKERN_ONLY

.include "${RUMPTOP}/Makefile.rump"
.include <bsd.lib.mk>
.include <bsd.klinks.mk>
