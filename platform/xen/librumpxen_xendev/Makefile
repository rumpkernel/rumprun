.include <bsd.own.mk>

LIB=	rumpxen_xendev

SRCS=	xendev_component.c
SRCS+=	busdev.c
# XXX: issue #73
#SRCS+=	evtdev.c
#SRCS+=	privcmd.c

RUMPTOP= ${TOPRUMP}

CPPFLAGS+=	-I${RUMPTOP}/librump/rumpkern -I${RUMPTOP}/librump
CPPFLAGS+=	-I${RUMPTOP}/librump/rumpvfs
CPPFLAGS+=	-I${.CURDIR}/../xen/include
CPPFLAGS+=	-I${.CURDIR}

RUMPCOMP_USER_SRCS=	busdev_user.c
RUMPCOMP_USER_CPPFLAGS+= -I${.CURDIR}/..
RUMPCOMP_USER_CPPFLAGS+= -I${.CURDIR}/../xen/include
RUMPCOMP_USER_CPPFLAGS+= -I${.CURDIR}/../../../include

.ifdef RUMP_DEV_XEN_DEBUG
CPPFLAGS+= -DRUMP_DEV_XEN_DEBUG=1
RUMPCOMP_USER_CPPFLAGS+= -DRUMP_DEV_XEN_DEBUG=1
.endif

# XXX
.undef RUMPKERN_ONLY

.include "${RUMPTOP}/Makefile.rump"
.include <bsd.lib.mk>
.include <bsd.klinks.mk>
