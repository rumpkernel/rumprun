#!/bin/sh

set -e

: ${BUILDRUMP:=../../buildrump.sh}

IFS=' '
BUILDXENMETAL_MKCONF=".if defined(LIB) && \${LIB} == \"pthread\"
.PATH:  $(pwd)/../../lib/librumprun_base/pthread
PTHREAD_MAKELWP=pthread_makelwp_rumprun.c
CPPFLAGS.pthread_makelwp_rumprun.c= -I$(pwd)/../../include
.endif  # LIB == pthread"
unset IFS

export BUILDXENMETAL_PCI_P='[ "${MACHINE}" = "amd64" -o "${MACHINE}" = "i386" ]'
export BUILDXENMETAL_PCI_ARGS='RUMP_PCI_IOSPACE=yes'
export BUILDXENMETAL_MKCONF

if [ ! -f ${BUILDRUMP}/subr.sh ]; then
	# old git versions need to run submodule in the repo root.
	(
		cd $(git rev-parse --show-cdup)
		git submodule update --init ${BUILDRUMP}
	)
fi
. ${BUILDRUMP}/subr.sh
../../_build-common.sh baremetal "$@" || die _build-common.sh failed

# build the image
make

echo
echo ">> $0 ran successfully"
exit 0
