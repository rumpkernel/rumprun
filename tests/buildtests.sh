#!/bin/sh

# This script builds all tests, including the one which
# tests for configure.  I'm sure it would be possible to do it
# with just "make", but a script seems vastly simpler now.

set -e

[ -n "${1}" ] || { echo '>> need platform' ; exit 1; }

cd "$(dirname $0)"
APPTOOLSDIR=$(pwd)/../app-tools
PLATFORM=$1

case ${PLATFORM} in
hw)
	TOOLS_PLATFORM=bmk
        RUMPBAKE="rumpbake -C hw_generic"
	;;
xen)
	TOOLS_PLATFORM=xen
        RUMPBAKE="rumpbake -C xen_pv"
	;;
*)
	echo ">> unknown platform \"$PLATFORM\""
	exit 1
esac
shift

# XXX
export DOCXX=$(grep ^CONFIG_CXX ../platform/${PLATFORM}/config.mk)

export MAKE=${APPTOOLSDIR}/rumprun-${TOOLS_PLATFORM}-make

${MAKE} ${DOCXX} RUMPBAKE="${APPTOOLSDIR}/${RUMPBAKE}"

if [ "$1" != '-q' ]; then
	cd configure
	${APPTOOLSDIR}/rumprun-${TOOLS_PLATFORM}-configure ./configure
	${MAKE}
fi

exit 0
