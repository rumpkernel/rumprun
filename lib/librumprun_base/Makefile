CONFIG_MK=	${.CURDIR}/../../config.mk
DPSRCS+=	${CONFIG_MK}
.include	"${CONFIG_MK}"

LIB=		rumprun_base
LIBISPRIVATE=	# defined

SRCS=		netbsd_initfini.c syscall_misc.c

CPPFLAGS+=	-I${.CURDIR}/../../include

.if ${CONFIG_SYSPROXY:U} == "yes"
CPPFLAGS+=	-DRUMP_SYSPROXY
SRCS+=		sysproxy.c
.endif

.include <bsd.lib.mk>
