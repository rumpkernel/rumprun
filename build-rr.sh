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

helpme ()
{

	echo "Usage: $0 [-s srcdir] [-q] hw|xen [-- buildrump.sh options]"
	printf "\t-s: specify alternative src-netbsd location (expert-only)\n"
	printf "\t-q: quiet(er) build.  option maybe be specified twice.\n\n"
	printf "buildrump.sh options are passed to buildrump.sh (expert-only)\n"
	printf "\n"
	printf "The toolchain is picked up from the environment.  See the\n"
	printf "rumprun wiki for more information.\n"
	exit 1
}

set -e

# defaults
STDJ='-j4'
RUMPSRC=src-netbsd
BUILDRUMP=$(pwd)/buildrump.sh

# figure out where gmake lies
if [ -z "${MAKE}" ]; then
	MAKE=make
	! type gmake >/dev/null 2>&1 || MAKE=gmake
fi
type ${MAKE} >/dev/null 2>&1 || die '"make" required but not found'


#
# SUBROUTINES
#

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
		'h'|'?')
			helpme
			exit 1
		esac
	done
	shift $((${OPTIND} - 1))

	[ $# -gt 0 ] || helpme

	PLATFORM=$1
	export PLATFORMDIR=platform/${PLATFORM}
	[ -d ${PLATFORMDIR} ] || die Platform \"$PLATFORM\" not supported!
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

	RUMPTOOLS=${PLATFORMDIR}/rumptools
	RUMPOBJ=${PLATFORMDIR}/rumpobj
	RUMPDEST=${PLATFORMDIR}/rump

	ARGSSHIFT=$((${orignargs} - $#))
}

checksubmodules ()
{

	# We assume that if the git submodule command fails, it's because
	# we're using external $RUMPSRC.
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
		echo -n '>>'
		for x in 1 2 3 4 5; do echo -n ' !' ; sleep 1 ; done
	fi
}

checkprevbuilds ()
{

	if [ -f .prevbuild ]; then
		. ./.prevbuild
		if [ "${PB_MACHINE}" != "${MACHINE}" \
		    -o "${PB_PLATFORM}" != "${PLATFORM}" ]; then
			echo '>> ERROR:'
			echo '>> Building for multiple machine/platform combos'
			echo '>> from the same rumprun source tree is currently'
			echo '>> not supported.  See rumprun issue #35.'
			printf '>> %20s: %s/%s\n' 'Previously built' \
			    ${PB_PLATFORM} ${PB_MACHINE}
			printf '>> %20s: %s/%s\n' 'Now building' \
			    ${PLATFORM} ${MACHINE}
			exit 1
		fi
	else
		echo PB_MACHINE=${MACHINE} > ./.prevbuild
		echo PB_PLATFORM=${PLATFORM} >> ./.prevbuild
	fi
}

buildrump ()
{

	# build tools
	${BUILDRUMP}/buildrump.sh ${BUILD_QUIET} ${STDJ} -k		\
	    -s ${RUMPSRC} -T ${RUMPTOOLS} -o ${RUMPOBJ} -d ${RUMPDEST}	\
	    -V MKPIC=no -V RUMP_CURLWP=__thread				\
	    -V RUMP_KERNEL_IS_LIBC=1 -V BUILDRUMP_SYSROOT=yes		\
	    "$@" tools

	echo '>>'
	echo '>> Now that we have the appropriate tools, perfoming'
	echo '>> further setup for rumprun build'
	echo '>>'

	RUMPMAKE=$(pwd)/${RUMPTOOLS}/rumpmake

	# Check that a clang build is not attempted.  This is the first
	# place we can do it without replicating buildrump.sh compiler
	# detection code
	HAVE_LLVM=$(${RUMPMAKE} -f /dev/null -V '${HAVE_LLVM}')
	[ ! "${HAVE_LLVM}" = "1" ] \
	    || die rumprun does not yet support clang ${CC:+(\$CC: $CC)}

	MACHINE=$(${RUMPMAKE} -f /dev/null -V '${MACHINE}')
	[ -n "${MACHINE}" ] || die could not figure out target machine

	checkprevbuilds

	makeconfigmk ${PLATFORMDIR}/config.mk

	cat >> ${RUMPTOOLS}/mk.conf << EOF
.if defined(LIB) && \${LIB} == "pthread"
.PATH:  $(pwd)/lib/librumprun_base/pthread
PTHREAD_MAKELWP=pthread_makelwp_rumprun.c
CPPFLAGS.pthread_makelwp_rumprun.c= -I$(pwd)/include
.endif  # LIB == pthread
EOF
	[ -z "${PLATFORM_MKCONF}" ] \
	    || echo "${PLATFORM_MKCONF}" >> ${RUMPTOOLS}/mk.conf

	TOOLTUPLE=$(${RUMPMAKE} -f bsd.own.mk \
	    -V '${MACHINE_GNU_PLATFORM:S/--netbsd/-rumprun-netbsd/}')
	echo "RUMPRUN_TUPLE=${TOOLTUPLE}" >> ${RUMPTOOLS}/mk.conf

	# build rump kernel
	${BUILDRUMP}/buildrump.sh ${BUILD_QUIET} ${STDJ} -k		\
	    -s ${RUMPSRC} -T ${RUMPTOOLS} -o ${RUMPOBJ} -d ${RUMPDEST}	\
	    "$@" build kernelheaders install

	echo '>>'
	echo '>> Rump kernel components built.  Proceeding to build'
	echo '>> rumprun bits'
	echo '>>'
}

builduserspace ()
{

	usermtree ${RUMPDEST}

	LIBS="$(stdlibs ${RUMPSRC})"
	! havecxx || LIBS="${LIBS} $(stdlibsxx ${RUMPSRC})"

	userincludes ${RUMPSRC} ${LIBS} $(pwd)/lib/librumprun_tester
	for lib in ${LIBS}; do
		makeuserlib ${lib}
	done
	makeuserlib $(pwd)/lib/librumprun_tester ${PLATFORM}

	# build unwind bits if we support c++
	if havecxx; then
		( cd lib/librumprun_unwind
		    ${RUMPMAKE} dependall && ${RUMPMAKE} install )
	fi
}

buildpci ()
{

	# need links to build the hypercall module
	${MAKE} -C ${PLATFORMDIR} links

	if eval ${PLATFORM_PCI_P}; then
		${RUMPMAKE} -f ${PLATFORMDIR}/pci/Makefile.pci ${STDJ} obj
		${RUMPMAKE} -f ${PLATFORMDIR}/pci/Makefile.pci ${STDJ} dependall
		${RUMPMAKE} -f ${PLATFORMDIR}/pci/Makefile.pci ${STDJ} install
	fi
}

wraponetool ()
{

	configfile=$1
	tool=$2

	tpath=$(${RUMPMAKE} -f bsd.own.mk -V "\${${tool}}")
	if ! [ -n "${tpath}" -a -x ${tpath} ]; then
		die Could not locate buildrump.sh tool \"${tool}\".
	fi
	echo "${tool}=${tpath}" >> ${configfile}
}

makeconfigmk ()
{

	echo "BUILDRUMP=${BUILDRUMP}" > ${1}
	echo "RUMPSRC=${RUMPSRC}" >> ${1}
	echo "RUMPMAKE=${RUMPMAKE}" >> ${1}
	echo "BUILDRUMP_TOOLFLAGS=$(pwd)/${RUMPTOOLS}/toolchain-conf.mk" >> ${1}
	echo "MACHINE=${MACHINE}" >> ${1}

	# wrap mandatory toolchain bits
	for t in AR AS CC CPP LD NM OBJCOPY OBJDUMP RANLIB READELF \
            SIZE STRINGS STRIP; do
		wraponetool ${1} ${t}
	done

	# c++ is optional, wrap it iff available
	if havecxx; then
		echo "CONFIG_CXX=yes" >> ${1}
		wraponetool ${1} CXX
	else
		echo "CONFIG_CXX=no" >> ${1}
	fi
}


#
# BEGIN SCRIPT
#

parseargs "$@"
shift ${ARGSSHIFT}

checksubmodules

. ${BUILDRUMP}/subr.sh
. ${PLATFORMDIR}/platform.conf

buildrump "$@"
builduserspace

# depends on config.mk
buildpci

# run routine specified in platform.conf
doextras || die 'platforms extras failed.  tillerman needs tea?'

# create high-level link to rumprun components
ln -sf ${PLATFORMDIR}/rump ./rumprun

# do final build of the platform bits
( cd ${PLATFORMDIR} && ${MAKE} || exit 1)
[ $? -eq 0 ] || die platform make failed!

# echo some useful information for the user
echo
echo '>>'
echo ">> Built rumprun for ${PLATFORM} : ${TOOLTUPLE}"
echo ">> cc: ${TOOLTUPLE}-$(${RUMPMAKE} -f bsd.own.mk -V '${ACTIVE_CC}')"
echo '>>'
echo ">> $0 ran successfully"
exit 0
