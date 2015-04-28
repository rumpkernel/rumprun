CONFIG_MK=	${.CURDIR}/../../config.mk
DPSRCS+=	${CONFIG_MK}
.include	"${CONFIG_MK}"

LIB=		rumprun_base
LIBISPRIVATE=	# defined

SRCS=		rumprun.c
SRCS+=		parseargs.c
SRCS+=		malloc.c netbsd_initfini.c signals.c syscall_misc.c
SRCS+=		__errno.c _lwp.c libc_stubs.c

CPPFLAGS+=	-I${.CURDIR}/../../include

.if ${CONFIG_SYSPROXY:U} == "yes"
CPPFLAGS+=	-DRUMP_SYSPROXY
SRCS+=		sysproxy.c
.endif

.include <bsd.lib.mk>
