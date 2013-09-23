#	$NetBSD$
#

RUMPTOP=${TOPRUMP}

.PATH:	${RUMPTOP}/../dev/pci ${RUMPTOP}/../dev/mii

LIB=	rumpdev_pci_if_wm
IOCONF=	IF_WM_PCI.ioconf

SRCS=	if_wm_at_pci.c

SRCS+=	if_wm.c

CPPFLAGS+= -I${RUMPTOP}/librump/rumpkern -I${.CURDIR}/opt
   
.include "${RUMPTOP}/Makefile.rump"
.include <bsd.lib.mk>
.include <bsd.klinks.mk>
