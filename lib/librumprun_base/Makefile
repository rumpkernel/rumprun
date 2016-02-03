LIB=		rumprun_base

SRCS=		main.c rumprun.c
SRCS+=		parseargs.c config.c
SRCS+=		malloc.c netbsd_initfini.c signals.c
SRCS+=		syscall_mman.c syscall_misc.c
SRCS+=		__errno.c _lwp.c libc_stubs.c
SRCS+=		daemon.c
SRCS+=		sysproxy.c

# doesn't really belong here, but at the moment we don't have
# a rumpkernel-only "userspace" lib
SRCS+=		genfs.c

INCS=		genfs.h
INCSDIR=	/usr/include/rumprun

WARNS=		5

CPPFLAGS+=	-I${.CURDIR}/../../include
CPPFLAGS+=	-D_KERNTYPES

.include <bsd.lib.mk>
