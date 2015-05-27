#! /usr/bin/env sh
#
# Copyright (c) 2014, 2015 Antti Kantee <pooka@iki.fi>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

die ()
{

	echo '>>' $*
	exit 1
}

set -e

# defaults
STDJ='-j4'
RUMPSRC=src-netbsd
BUILDRUMP=$(pwd)/buildrump.sh

parseargs ()
{

	orignargs=$#
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

	case ${RUMPSRC} in
	/*)
		;;
	*)
		RUMPSRC=$(pwd)/${RUMPSRC}
		;;
	esac

	export RUMPSRC
	export BUILD_QUIET

	ARGSSHIFT=$((${orignargs} - $#))
}

checksubmodules ()
{

	# old git versions need to run submodule in the repo root. *sheesh*
	# We assume that if the git submodule command fails, it's because
	# we're using external $RUMPSRC
	( cd $(git rev-parse --show-cdup)
	if git submodule status ${RUMPSRC} 2>/dev/null | grep -q '^-' \
	    || git submodule status ${BUILDRUMP} 2>/dev/null | grep -q '^-';
	then
		echo '>>'
		echo '>> submodules missing.  run "git submodule update --init"'
		echo '>>'
		exit 1
	fi

	if git submodule status ${RUMPSRC} 2>/dev/null | grep -q '^+' \
	    || git submodule status ${BUILDRUMP} 2>/dev/null | grep -q '^+'
	then
		echo '>>'
		echo '>> Your git submodules are out-of-date'
		echo '>> Forgot to run "git submodule update" after pull?'
		echo '>> (sleeping for 5s, press ctrl-C to abort)'
		echo '>>'
		sleep 5
	fi )
}

parseargs "$@"
shift ${ARGSSHIFT}

checksubmodules

. ${BUILDRUMP}/subr.sh
. ${PLATFORMDIR}/platform.conf

rumptools=${PLATFORMDIR}/rumptools
rumpobj=${PLATFORMDIR}/rumpobj
rumpdest=${PLATFORMDIR}/rump

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
