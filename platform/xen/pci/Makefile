RUMPTOP= ${TOPRUMP}

.include "${RUMPRUN_MKCONF}"
.include "${BUILDRUMP_TOOLFLAGS}"

RUMPCOMP_MAKEFILEINC_rumpdev_pci:= ${.PARSEDIR}/Makefile.pcihyperdefs
.export RUMPCOMP_MAKEFILEINC_rumpdev_pci

RUMPRUN_OBJDIR=${RROBJ}
RUMPRUN_TOOL_CFLAGS=${BUILDRUMP_TOOL_CFLAGS}

.export RUMPRUN_OBJDIR
.export RUMPRUN_TOOL_CFLAGS

.include "${RUMPTOP}/dev/Makefile.rumpdevcomp"

.for pcidev in ${RUMPPCIDEVS}
SUBDIR+= ${RUMPTOP}/dev/lib/lib${pcidev}
.endfor

.include <bsd.subdir.mk>
