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
		echo "$0 [-k|-q]"
		exit 1
	esac
done
shift $((${OPTIND} - 1))

[ -n "${RUMPRUN_SHCONF}" ] || { echo '>> need RUMPRUN_SHCONF'; exit 1; }
. "${RUMPRUN_SHCONF}"

cd "$(dirname $0)"

test_apptools()
{

	case ${PLATFORM} in
	hw)
		RUMPBAKE="rumprun-bake hw_generic"
		;;
	xen)
		RUMPBAKE="rumprun-bake xen_pv"
		;;
	*)
		echo ">> unknown platform \"$PLATFORM\""
		exit 1
	esac

	export PATH="${PATH}:${RRDEST}/bin"
	export CC="${RRDEST}/bin/${TOOLTUPLE}-gcc"
	export CXX="${RRDEST}/bin/${TOOLTUPLE}-g++"
	export CONFIG_CXX
	export RUMPBAKE

	make

	if ${TESTCONFIGURE}; then
		(
			cd configure
			./configure --host=${TOOLTUPLE}
			make
		)
	fi

	if [ -n "${TESTCMAKE}" ]; then
		(
			mkdir -p cmake/build
			cd cmake/build
			cmake -DCMAKE_TOOLCHAIN_FILE=${RRDEST}/share/${TOOLTUPLE}-toolchain.cmake ..
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
