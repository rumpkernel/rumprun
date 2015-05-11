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

shift
if [ $# -gt 0 ]; then
	if [ $1 = '--' ]; then
		shift
	else
		die Invalid argument: $1
	fi
fi

export BUILDRUMP=$(pwd)/buildrump.sh
case ${RUMPSRC} in
/*)
	;;
*)
	RUMPSRC=$(pwd)/${RUMPSRC}
	;;
esac
export RUMPSRC
export BUILD_QUIET

( cd platform/${platform} && ./${script} -V RUMP_CURLWP=__thread "$@" )
[ $? -eq 0 ] || die Build script \"$script\" failed!

ln -sf platform/${platform}/rump .
