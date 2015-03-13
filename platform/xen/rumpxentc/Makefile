LIB=	rumpxen_tc

SRCS+=	rumpxen_tc.c

RUMPTOP= ${TOPRUMP}

CPPFLAGS+=	-I${RUMPTOP}/librump/rumpkern -I${RUMPTOP}/librump
CPPFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../rump/include
CPPFLAGS+=	-I${.CURDIR}/../xen/include -D__RUMP_KERNEL__ -I${.CURDIR}/..

RUMP_SYM_NORENAME=xenbus_|HYPERVISOR_|minios_

.include "${RUMPTOP}/Makefile.rump"
.include <bsd.lib.mk>
.include <bsd.klinks.mk>
