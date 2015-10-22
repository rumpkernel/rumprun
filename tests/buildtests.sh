#!/bin/sh

# This script builds all tests, including the one which
# tests for configure.  I'm sure it would be possible to do it
# with just "make", but a script seems vastly simpler now.

# we know, we knooooooow
export RUMPRUN_WARNING_STFU=please

set -e

TESTMODE=apptools
TESTCONFIGURE=true
TESTCMAKE=$(which cmake || echo "")

while getopts 'kqh' opt; do
	case "$opt" in
	'k')
		TESTMODE=kernonly
		;;
	'q')
		TESTCONFIGURE=false
		;;
	'h'|'?')
		echo "$0 [-k|-q] MACHINE PLATFORM [ELF]"
		exit 1
	esac
done
shift $((${OPTIND} - 1))

[ -n "${RUMPRUN_SHCONF}" ] || { echo '>> need RUMPRUN_SHCONF'; exit 1; }
. "${RUMPRUN_SHCONF}"

cd "$(dirname $0)"

test_apptools()
{
	APPTOOLSDIR=$(pwd)/../app-tools

	case ${PLATFORM} in
	hw)
		RUMPBAKE="rumpbake hw_generic"
		;;
	xen)
		RUMPBAKE="rumpbake xen_pv"
		;;
	*)
		echo ">> unknown platform \"$PLATFORM\""
		exit 1
	esac

	export MAKE=${APPTOOLSDIR}/${TOOLTUPLE}-${MAKE-make}

	${MAKE} CONFIG_CXX=${CONFIG_CXX} RUMPBAKE="${APPTOOLSDIR}/${RUMPBAKE}"

	if ${TESTCONFIGURE}; then
		(
			cd configure
			${APPTOOLSDIR}/${TOOLTUPLE}-configure ./configure
			${MAKE}
		)
	fi

	if [ -n "${TESTCMAKE}" ]; then
		(
			mkdir -p cmake/build
			cd cmake/build
			cmake -DCMAKE_TOOLCHAIN_FILE=${APPTOOLSDIR}/${TOOLTUPLE}-toolchain.cmake ..
			make
		)
	fi
}

test_kernonly()
{
	if [ -z "${MAKE}" ]; then
		MAKE=make
		! type gmake >/dev/null 2>&1 || MAKE=gmake
	fi

	${MAKE} PLATFORM=${PLATFORM} kernonly-tests
}

test_${TESTMODE}

exit 0
