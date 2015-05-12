#!/bin/sh

set -e

: ${BUILDRUMP:=../../buildrump.sh}

IFS=' '
BUILDXENMETAL_MKCONF="# maxphys = 32k is a Xen limitation (64k - overhead)
CPPFLAGS+=-DMAXPHYS=32768
.if defined(LIB) && \${LIB} == \"pthread\"
.PATH:	$(pwd)/../../lib/librumprun_base/pthread
PTHREAD_MAKELWP=pthread_makelwp_rumprun.c
CPPFLAGS.pthread_makelwp_rumprun.c= -I$(pwd)/../../include
.endif  # LIB == pthread
CFLAGS+=-fno-stack-protector -fno-builtin-sin -fno-builtin-cos
CFLAGS+=-fno-builtin-sinf -fno-builtin-cosf"
unset IFS

export BUILDXENMETAL_MKCONF
export BUILDXENMETAL_PCI_P=true

if [ ! -f ${BUILDRUMP}/subr.sh ]; then
	# old git versions need to run submodule in the repo root.
	(
		cd $(git rev-parse --show-cdup)
		git submodule update --init ${BUILDRUMP}
	)
fi
. ${BUILDRUMP}/subr.sh
../../_build-common.sh xen "$@" || die _build-common.sh failed

RUMPMAKE=$(pwd)/rumptools/rumpmake

makekernlib ()
{
	lib=$1
	OBJS=`pwd`/rumpobj/$lib
	mkdir -p ${OBJS}
	( cd ${lib} &&
		${RUMPMAKE} MAKEOBJDIRPREFIX=${OBJS} obj &&
		${RUMPMAKE} MAKEOBJDIRPREFIX=${OBJS} dependall &&
		${RUMPMAKE} MAKEOBJDIRPREFIX=${OBJS} install
	) || die makekernlib $lib failed
}
make -C xen links
makekernlib rumpxenif
makekernlib rumpxendev
makekernlib rumpxentc

# build the domU image
make || die make failed

echo
echo ">> $0 ran successfully"
exit 0
