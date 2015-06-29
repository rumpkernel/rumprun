LIB=		rumprun_base
LIBISPRIVATE=	# defined

SRCS=		rumprun.c
SRCS+=		parseargs.c rumpconfig.c
SRCS+=		malloc.c netbsd_initfini.c signals.c
SRCS+=		syscall_mman.c syscall_misc.c
SRCS+=		__errno.c _lwp.c libc_stubs.c
SRCS+=		sysproxy.c

CPPFLAGS+=	-I${.CURDIR}/../../include

.include <bsd.lib.mk>
