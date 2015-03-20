#!/bin/sh

#
# some common crud waiting for the build scripts to be unified.
# will go away eventually
#
# accepts the following extra manglers:
#
#	BUILDXENMETAL_MKCONF:	extra contents for mk.conf
#	BUILDXENMETAL_PCI_P:	predicate for building PCI drivers
#	BUILDXENMETAL_PCI_ARGS:	extra build args to PCI build
#

STDJ='-j4'
: ${RUMPSRC=./rumpsrc}
: ${BUILDRUMP:=./buildrump.sh}

platform=$1
shift

# the buildxen.sh is not as forgiving as I am
set -e

. ${BUILDRUMP}/subr.sh

# old git versions need to run submodule in the repo root. *sheesh*
(
	cd $(git rev-parse --show-cdup)
	if git submodule status ${RUMPSRC} | grep -q '^-' ; then
		git submodule update --init --recursive ${RUMPSRC}
	fi
)
[ "$1" = "justcheckout" ] && { echo ">> $0 done" ; exit 0; }

# build tools
${BUILDRUMP}/buildrump.sh ${BUILD_QUIET} ${STDJ} -k \
    -V MKPIC=no -s ${RUMPSRC} -T rumptools -o rumpobj -N \
    -V RUMP_KERNEL_IS_LIBC=1 -V BUILDRUMP_SYSROOT=yes "$@" tools

[ -n "${BUILDXENMETAL_MKCONF}" ] \
    && echo "${BUILDXENMETAL_MKCONF}" >> rumptools/mk.conf

RUMPMAKE=$(pwd)/rumptools/rumpmake
MACHINE=$(${RUMPMAKE} -f /dev/null -V '${MACHINE}')
[ -z "${MACHINE}" ] && die could not figure out target machine

# build rump kernel
${BUILDRUMP}/buildrump.sh ${BUILD_QUIET} ${STDJ} -k \
    -V MKPIC=no -s ${RUMPSRC} -T rumptools -o rumpobj -N \
    -V RUMP_KERNEL_IS_LIBC=1 -V BUILDRUMP_SYSROOT=yes "$@" \
    build kernelheaders install

LIBS="$(stdlibs ${RUMPSRC})"
if [ "$(${RUMPMAKE} -f rumptools/mk.conf -V '${_BUILDRUMP_CXX}')" = 'yes' ]
then
	LIBS="${LIBS} $(stdlibsxx ${RUMPSRC})"
fi

usermtree rump
userincludes ${RUMPSRC} ${LIBS}

for lib in ${LIBS}; do
	makeuserlib ${lib}
done

eval ${BUILDXENMETAL_PCI_P} && makepci ${RUMPSRC} ${BUILDXENMETAL_PCI_ARGS}

# build unwind bits if we support c++
if havecxx; then
        ( cd ../../lib/librumprun_unwind
	    ${RUMPMAKE} dependall && ${RUMPMAKE} install )
        CONFIG_CXX=yes
else
        CONFIG_CXX=no
fi

( cd ../../lib/librumprun_base
    ${RUMPMAKE} MAKEOBJDIR=${platform} obj \
      && ${RUMPMAKE} MAKEOBJDIR=${platform} dependall \
      && ${RUMPMAKE} MAKEOBJDIR=${platform} install )

echo "BUILDRUMP=${BUILDRUMP}" > config.mk
echo "RUMPSRC=${RUMPSRC}" >> config.mk
echo "CONFIG_CXX=${CONFIG_CXX}" >> config.mk
echo "RUMPMAKE=${RUMPMAKE}" >> config.mk

tools="AR CPP CC INSTALL NM OBJCOPY"
havecxx && tools="${tools} CXX"
for t in ${tools}; do
	echo "${t}=$(${RUMPMAKE} -f bsd.own.mk -V "\${${t}}")" >> config.mk
done

exit 0
