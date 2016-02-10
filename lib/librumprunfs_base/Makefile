# XXX: this Makefile is built with rumpmake

include ${RUMPRUN_MKCONF}
include ${BUILDRUMP_TOOLFLAGS}

obj:
	mkdir -p ${MAKEOBJDIR}

includes:

dependall:
	( cd ${.CURDIR} &&						\
	  RUMPRUN_GENFS_CC="${CC}" RUMPRUN_GENFS_OBJCOPY="${OBJCOPY}"	\
	    RUMPRUN_GENFS_INCDIR="${RROBJ}/dest.stage/include"		\
		${RROBJ}/app-tools/${TOOLTUPLE}-cookfs			\
		    ${MAKEOBJDIR}/librumprunfs_base.a etc )
