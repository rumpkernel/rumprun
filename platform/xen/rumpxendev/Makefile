LIB=	rumpxen_xendev

SRCS=	xendev_component.c
SRCS+=	busdev.c

RUMPTOP= ${TOPRUMP}

CPPFLAGS+=	-I${RUMPTOP}/librump/rumpkern -I${RUMPTOP}/librump
CPPFLAGS+=	-I${RUMPTOP}/librump/rumpvfs
CPPFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../rump/include
CPPFLAGS+=	-I${.CURDIR}/../xen/include -D__RUMP_KERNEL__ -I${.CURDIR}/..
CPPFLAGS+=	-I${.CURDIR}/../../../include

RUMP_SYM_NORENAME=xenbus_|HYPERVISOR_|minios_|bmk_

.include "${RUMPTOP}/Makefile.rump"
.include <bsd.lib.mk>
.include <bsd.klinks.mk>
