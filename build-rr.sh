#!/bin/sh

die ()
{

	echo '>>' $*
	exit 1
}

RUMPSRC=src-netbsd
while getopts '?qs:' opt; do
	case "$opt" in
	's')
		RUMPSRC=${OPTARG}
		;;
	'q')
		BUILD_QUIET=${BUILD_QUIET:=-}q
		;;
	'?')
		echo HELP!
		exit 1
	esac
done
shift $((${OPTIND} - 1))

[ $# -gt 0 ] || die Need platform argument
platform=$1
shift

case ${platform} in
'baremetal')
	script=buildme.sh
	;;
'xen')
	script=buildxen.sh
	;;
*)
	die Platform \"$platform\" not supported!
	;;
esac

export BUILDRUMP=$(pwd)/buildrump.sh
case ${RUMPSRC} in
/*)
	;;
*)
	RUMPSRC=$(pwd)/${RUMPSRC}
	;;
esac
export RUMPSRC

( cd platform/${platform} && ./${script} "$@" )
[ $? -eq 0 ] || die Build script \"$script\" failed!

ln -sf platform/${platform}/rump .
ln -sf platform/${platform}/app-tools .
