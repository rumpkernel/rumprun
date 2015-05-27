#!/bin/sh

die ()
{

	echo '>>' $*
	exit 1
}

set -e

# defaults
STDJ='-j4'
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

rumptools=${PLATFORMDIR}/rumptools
rumpobj=${PLATFORMDIR}/rumpobj
rumpdest=${PLATFORMDIR}/rump

# old git versions need to run submodule in the repo root. *sheesh*
# We assume that if the git submodule command fails, it's because
# we're using external $RUMPSRC
(
	cd $(git rev-parse --show-cdup)
	if git submodule status ${RUMPSRC} 2>/dev/null | grep -q '^-' ; then
		echo '>>'
		echo '>> src-netbsd missing.  run "git submodule update --init"'
		echo '>>'
		exit 1
	fi
	if git submodule status ${RUMPSRC} 2>/dev/null | grep -q '^+' ; then
		echo '>>'
		echo '>> Your git submodules are out-of-date'
		echo '>> Did you forget to run "git submodule update"?'
		echo '>> (sleeping for 5s)'
		echo '>>'
		sleep 5
	fi
)

# build tools
${BUILDRUMP}/buildrump.sh ${BUILD_QUIET} ${STDJ} -k \
    -V MKPIC=no -s ${RUMPSRC} -T ${rumptools} -o ${rumpobj} -d ${rumpdest} \
    -V RUMP_CURLWP=__thread \
    -V RUMP_KERNEL_IS_LIBC=1 -V BUILDRUMP_SYSROOT=yes "$@" tools

[ -n "${BUILDXENMETAL_MKCONF}" ] \
    && echo "${BUILDXENMETAL_MKCONF}" >> ${rumptools}/mk.conf

RUMPMAKE=$(pwd)/${rumptools}/rumpmake
MACHINE=$(${RUMPMAKE} -f /dev/null -V '${MACHINE}')
[ -z "${MACHINE}" ] && die could not figure out target machine

# build rump kernel
${BUILDRUMP}/buildrump.sh ${BUILD_QUIET} ${STDJ} -k \
    -V MKPIC=no -s ${RUMPSRC} -T ${rumptools} -o ${rumpobj} -d ${rumpdest} \
    -V RUMP_CURLWP=__thread \
    -V RUMP_KERNEL_IS_LIBC=1 -V BUILDRUMP_SYSROOT=yes "$@" \
    build kernelheaders install

LIBS="$(stdlibs ${RUMPSRC}) $(pwd)/lib/librumprun_tester"
if [ "$(${RUMPMAKE} -f ${rumptools}/mk.conf -V '${_BUILDRUMP_CXX}')" = 'yes' ]
then
	LIBS="${LIBS} $(stdlibsxx ${RUMPSRC})"
fi

usermtree ${rumpdest}
userincludes ${RUMPSRC} ${LIBS}

for lib in ${LIBS}; do
	makeuserlib ${lib}
done

eval ${BUILDXENMETAL_PCI_P} && makepci ${RUMPSRC} ${BUILDXENMETAL_PCI_ARGS}

# build unwind bits if we support c++
if havecxx; then
        ( cd lib/librumprun_unwind
	    ${RUMPMAKE} dependall && ${RUMPMAKE} install )
        CONFIG_CXX=yes
else
        CONFIG_CXX=no
fi

configmk=${PLATFORMDIR}/config.mk

echo "BUILDRUMP=${BUILDRUMP}" > ${configmk}
echo "RUMPSRC=${RUMPSRC}" >> ${configmk}
echo "CONFIG_CXX=${CONFIG_CXX}" >> ${configmk}
echo "RUMPMAKE=${RUMPMAKE}" >> ${configmk}
echo "BUILDRUMP_TOOLFLAGS=$(pwd)/${rumptools}/toolchain-conf.mk" >> ${configmk}
echo "MACHINE=${MACHINE}" >> ${configmk}

tools="AR CPP CC INSTALL NM OBJCOPY"
havecxx && tools="${tools} CXX"
for t in ${tools}; do
	echo "${t}=$(${RUMPMAKE} -f bsd.own.mk -V "\${${t}}")" >> ${configmk}
done

export RUMPMAKE=$(pwd)/${PLATFORMDIR}/rumptools/rumpmake
doextras || die 'platforms extras failed.  tillerman needs tea?'

( cd ${PLATFORMDIR} && make || exit 1)
[ $? -eq 0 ] || die platform make failed!

ln -sf ${PLATFORMDIR}/rump .

echo
echo ">> $0 ran successfully"

exit 0
