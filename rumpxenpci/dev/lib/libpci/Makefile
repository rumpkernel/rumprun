#	$NetBSD$
#

RUMPTOP= ${TOPRUMP}

.PATH:	${RUMPTOP}/../dev/pci

LIB=	rumpdev_pci
IOCONF=	PCI.ioconf

SRCS+=	pci.c pci_map.c pci_quirks.c pci_subr.c pci_stub.c pci_usrreq.c
SRCS+=	pcibusprint.c

SRCS+=	rumpdev_pci.c

# ok, these don't _really_ belong here, but it's the only
# place they're currently useful, so let it slide
SRCS+=	rumpdev_bus_space.c
SRCS+=	rumpdev_bus_dma.c

SRCS+=	pci_at_mainbus.c

CPPFLAGS+= -I${.CURDIR}/opt -I${RUMPTOP}/librump/rumpkern

.include "${RUMPTOP}/Makefile.rump"
.include <bsd.lib.mk>
.include <bsd.klinks.mk>
