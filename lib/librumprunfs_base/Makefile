# XXX: this Makefile is built with rumpmake

include ${RUMPRUN_MKCONF}
include ${BUILDRUMP_TOOLFLAGS}

obj:
	mkdir -p ${MAKEOBJDIR}

includes:

dependall:
	( cd ${.CURDIR} &&						\
	  RUMPRUN_COOKFS_CC="${CC}"					\
	  RUMPRUN_COOKFS_OBJCOPY="${OBJCOPY}"				\
	  RUMPRUN_COOKFS_SIZE="${SIZE}"					\
	    RUMPRUN_COOKFS_INCDIR="${RROBJ}/dest.stage/include"		\
		${RROBJ}/app-tools/${TOOLTUPLE}-cookfs -s 1		\
		    ${MAKEOBJDIR}/librumprunfs_base.a rootfs )

install:
	mkdir -p ${DESTDIR}/share/rumprun/rumprunfs_base
	( cd ${.CURDIR}/rootfs ; tar -cf - . )				\
	    | ( cd ${DESTDIR}/share/rumprun/rumprunfs_base ; tar -xf - )
	install -m 444 ${MAKEOBJDIR}/librumprunfs_base.a ${LIBDIR}
