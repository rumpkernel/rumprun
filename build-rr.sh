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
export RUMPSRC=$(pwd)/src-netbsd
( cd platform/${platform} && ./${script} "$@" )
[ $? -eq 0 ] || die Build script \"$script\" failed!

ln -sf platform/${platform}/rump .
ln -sf platform/${platform}/app-tools .
