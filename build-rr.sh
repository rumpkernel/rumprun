#!/bin/sh

die ()
{

	echo '>>' $*
	exit 1
}

[ $# -gt 0 ] || die Must give platform as first argument!
platform=$1
shift

case ${platform} in
'baremetal')
	script=platform/baremetal/buildme.sh
	result=platform/baremetal/rump
	;;
'xen')
	script=platform/xen/buildxen.sh
	result=platform/xen/rump
	;;
*)
	die Platform \"$platform\" not supported!
	;;
esac

export BUILDRUMP=$(pwd)/buildrump.sh
export RUMPSRC=$(pwd)/rumpsrc
( cd $(dirname ${script}) && ./$(basename ${script}) "$@" )
[ $? -eq 0 ] || die Build script \"$script\" failed!

ln -s ${result} ./rump
