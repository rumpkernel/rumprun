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
baremetal)
	TOOLS_PLATFORM=bmk
	;;
xen)
	TOOLS_PLATFORM=xen
	;;
*)
	echo ">> unknown platform \"$PLATFORM\""
	exit 1
esac

export MAKE=${APPTOOLSDIR}/rumprun-${TOOLS_PLATFORM}-make

${MAKE}

cd configure
${APPTOOLSDIR}/rumprun-${TOOLS_PLATFORM}-configure ./configure
${MAKE}

exit 0
