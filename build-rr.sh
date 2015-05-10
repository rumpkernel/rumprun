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
	USETLS='-V RUMP_CURLWP=__thread'
	script=buildme.sh
	;;
'xen')
	# does not work on 32bit Xen yet, and at this stage we don't
	# have any easy way to detect if we're building for x32 or x64
	# since the compiler is determined by buildrump.sh and we haven't
	# run that yet
	#USETLS='-V RUMP_CURLWP=__thread'

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

( cd platform/${platform} && ./${script} ${USETLS} "$@" )
[ $? -eq 0 ] || die Build script \"$script\" failed!

ln -sf platform/${platform}/rump .
