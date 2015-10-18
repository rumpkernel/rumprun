LIB=		rumprun_base

SRCS=		main.c rumprun.c
SRCS+=		parseargs.c config.c
SRCS+=		malloc.c netbsd_initfini.c signals.c
SRCS+=		syscall_mman.c syscall_misc.c
SRCS+=		__errno.c _lwp.c libc_stubs.c
SRCS+=		sysproxy.c

WARNS=		5

CPPFLAGS+=	-I${.CURDIR}/../../include

.include <bsd.lib.mk>
