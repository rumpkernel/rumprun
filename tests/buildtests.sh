#!/bin/sh

# This script builds all tests, including the one which
# tests for configure.  I'm sure it would be possible to do it
# with just "make", but a script seems vastly simpler now.

set -e

[ -z "${RUMPRUN_PLATFORM}" ] && { echo '>> set $RUMPRUN_PLATFORM' ; exit 1; }

cd "$(dirname $0)"
APPTOOLSDIR=$(pwd)/../app-tools
export MAKE=${APPTOOLSDIR}/rumprun-${RUMPRUN_PLATFORM}-make

${MAKE}

cd configure
${APPTOOLSDIR}/rumprun-${RUMPRUN_PLATFORM}-configure ./configure
${MAKE}

exit 0
