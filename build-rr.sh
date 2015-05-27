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
export PLATFORMDIR=platform/${platform}
[ -d ${PLATFORMDIR} ] || die Platform \"$platform\" not supported!

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

if [ ! -f ${BUILDRUMP}/subr.sh ]; then
	# old git versions need to run submodule in the repo root.
	(
		cd $(git rev-parse --show-cdup)
		git submodule update --init ${BUILDRUMP}
	)
fi
. ${BUILDRUMP}/subr.sh

. ${PLATFORMDIR}/platform.conf
./_build-common.sh -V RUMP_CURLWP=__thread "$@" || die _build-common.sh failed

export RUMPMAKE=$(pwd)/${PLATFORMDIR}/rumptools/rumpmake
doextras || die 'platforms extras failed.  tillerman needs tea?'

( cd ${PLATFORMDIR} && make || exit 1)
[ $? -eq 0 ] || die platform make failed!

ln -sf ${PLATFORMDIR}/rump .

echo
echo ">> $0 ran successfully"
exit 0
