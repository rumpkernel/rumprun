#!/bin/sh

# This script builds all tests, including the one which
# tests for configure.  I'm sure it would be possible to do it
# with just "make", but a script seems vastly simpler now.

# we know, we knooooooow
export RUMPRUN_WARNING_STFU=please

set -e

TESTMODE=apptools
TESTCONFIGURE=true

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

[ -n "${1}" ] || { echo '>> need machine' ; exit 1; }
[ -n "${2}" ] || { echo '>> need platform' ; exit 1; }

cd "$(dirname $0)"
MACHINE=$1
PLATFORM=$2
ELF=$3

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

	# XXX
	export DOCXX=$(grep ^CONFIG_CXX ../platform/${PLATFORM}/config.mk)

	export MAKE=${APPTOOLSDIR}/${MACHINE}-rumprun-netbsd${ELF}-${MAKE-make}

	${MAKE} ${DOCXX} RUMPBAKE="${APPTOOLSDIR}/${RUMPBAKE}"

	if ${TESTCONFIGURE}; then
		cd configure
		${APPTOOLSDIR}/${MACHINE}-rumprun-netbsd${ELF}-configure ./configure
		${MAKE}
	fi
}

test_kernonly()
{
	if [ -z "${MAKE}" ]; then
		MAKE=make
		! type gmake >/dev/null 2>&1 || MAKE=gmake
	fi

	${MAKE} kernonly-tests
}

test_${TESTMODE}

exit 0
