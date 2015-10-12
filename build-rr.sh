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

set -u

die ()
{

	echo '>>' $*
	exit 1
}

helpme ()
{

	printf "Usage: $0 [-j num] [-k] [-o objdir] [-q] [-s srcdir] hw|xen\n"
	printf "\t    [build] [install] [-- buildrump.sh opts]\n"
	printf "\n"
	printf "\t-d: destination base directory (under construction).\n"
	printf "\t-j: run <num> make jobs simultaneously.\n"
	printf "\t-q: quiet(er) build.  option may be specified twice.\n\n"
	printf "\tThe default actions are \"build\" and \"install\"\n\n"

	printf "Expert-only options:\n"
	printf "\t-o: use non-default object directory (under development)\n"
	printf "\t-k: build kernel only, without libc or tools (expert-only)\n"
	printf "\t-s: specify alternative src-netbsd location (expert-only)\n\n"
	printf "\tbuildrump.sh opts are passed to buildrump.sh (expert-only)\n"
	printf "\n"
	printf "The toolchain is picked up from the environment.  See the\n"
	printf "Rumprun wiki for more information.\n"
	exit 1
}

set -e

BUILDRUMP=$(pwd)/buildrump.sh

# overriden by script if true
HAVECXX=false

# figure out where gmake lies
if [ -z "${MAKE:-}" ]; then
	MAKE=make
	! type gmake >/dev/null 2>&1 || MAKE=gmake
fi
type ${MAKE} >/dev/null 2>&1 || die '"make" required but not found'


#
# SUBROUTINES
#

abspath ()
{

	eval mypath=\${$1}
	case ${mypath} in
	/*)
		;;
	*)
		mypath="$(pwd)/${mypath}"
	esac

	eval ${1}="\${mypath}"
}

parseargs ()
{

	DESTDIR=./rumprun
	KERNONLY=false
	RROBJ=
	RUMPSRC=src-netbsd
	STDJ=-j4

	DObuild=false
	DOinstall=false

	orignargs=$#
	while getopts '?d:hj:ko:qs:' opt; do
		case "$opt" in
		'j')
			[ -z "$(echo ${OPTARG} | tr -d '[0-9]')" ] \
			    || die argument to -j must be a number
			STDJ=-j${OPTARG}
			;;
		'd')
			DESTDIR="${OPTARG}"
			;;
		'k')
			KERNONLY=true
			;;
		'o')
			RROBJ="${OPTARG}"
			;;
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

	: ${BUILD_QUIET:=}

	[ $# -gt 0 ] || helpme

	PLATFORM=$1
	export PLATFORMDIR=platform/${PLATFORM}
	[ -d ${PLATFORMDIR} ] || die Platform \"$PLATFORM\" not supported!
	shift

	if ${KERNONLY} && [ "${PLATFORM}" != "hw" ]; then
		die '-k currently only supports "hw" platform'
	fi

	dodefault=true
	while [ $# -gt 0 ]; do
		if [ $1 = '--' ]; then
			shift
			break
		else
			case $1 in
			build|install)
				eval DO${1}=true
				;;
			*)
				die invalid argument $1
				;;
			esac
			dodefault=false
			shift
		fi
	done
	if ${dodefault}; then
		DObuild=true
		DOinstall=true
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
		: ${PB_KERNONLY:=false} # "bootstrap", remove in a few months
		if [ "${PB_MACHINE}" != "${MACHINE}" \
		    -o "${PB_PLATFORM}" != "${PLATFORM}" \
		    -o "${PB_KERNONLY}" != "${KERNONLY}" \
		]; then
			echo '>> ERROR:'
			echo '>> Building for multiple machine/platform combos'
			echo '>> from the same rumprun source tree is currently'
			echo '>> not supported.  See rumprun issue #35.'
			printf '>> %20s: %s/%s nolibc=%s\n' 'Previously built' \
			    ${PB_PLATFORM} ${PB_MACHINE} ${PB_KERNONLY}
			printf '>> %20s: %s/%s nolibc=%s\n' 'Now building' \
			    ${PLATFORM} ${MACHINE} ${KERNONLY}
			exit 1
		fi
	else
		echo PB_MACHINE=${MACHINE} > ./.prevbuild
		echo PB_PLATFORM=${PLATFORM} >> ./.prevbuild
		echo PB_KERNONLY=${KERNONLY} >> ./.prevbuild
	fi
}

setvars ()
{

	. ${RUMPTOOLS}/proberes.sh
	MACHINE="${BUILDRUMP_MACHINE}"

	if [ -z "${RROBJ}" ]; then
		RROBJ="./obj-${PLATFORM}-${MACHINE}"
	fi
	STAGING="${RROBJ}/dest.stage"
	BROBJ="${RROBJ}/buildrump.sh"

	abspath DESTDIR
	abspath RROBJ
	abspath RUMPSRC
}

buildrump ()
{

	# probe
	${BUILDRUMP}/buildrump.sh -k -s ${RUMPSRC} -T ${RUMPTOOLS} "$@" probe

	setvars

	# Check that a clang build is not attempted.
	[ -z "${BUILDRUMP_HAVE_LLVM}" ] \
	    || die rumprun does not yet support clang ${CC:+(\$CC: $CC)}

	checkprevbuilds

	extracflags=
	[ "${MACHINE}" = "amd64" ] && extracflags='-F CFLAGS=-mno-red-zone'

	# build tools
	${BUILDRUMP}/buildrump.sh ${BUILD_QUIET} ${STDJ} -k		\
	    -s ${RUMPSRC} -T ${RUMPTOOLS} -o ${BROBJ} -d ${STAGING}	\
	    -V MKPIC=no -V RUMP_CURLWP=__thread				\
	    -V RUMP_KERNEL_IS_LIBC=1 -V BUILDRUMP_SYSROOT=yes		\
	    ${extracflags} "$@" tools

	echo '>>'
	echo '>> Now that we have the appropriate tools, performing'
	echo '>> further setup for rumprun build'
	echo '>>'

	RUMPMAKE=$(pwd)/${RUMPTOOLS}/rumpmake

	MACHINE_ARCH=$(${RUMPMAKE} -f /dev/null -V '${MACHINE_ARCH}')
	[ -n "${MACHINE_ARCH}" ] || die could not figure out target machine arch

	[ $(${RUMPMAKE} -f bsd.own.mk -V '${_BUILDRUMP_CXX}') != 'yes' ] \
	    || HAVECXX=true

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
	    -s ${RUMPSRC} -T ${RUMPTOOLS} -o ${BROBJ} -d ${STAGING}	\
	    "$@" build kernelheaders install

	echo '>>'
	echo '>> Rump kernel components built.  Proceeding to build'
	echo '>> rumprun bits'
	echo '>>'
}

builduserspace ()
{

	usermtree ${STAGING}

	LIBS="$(stdlibs ${RUMPSRC})"
	! ${HAVECXX} || LIBS="${LIBS} $(stdlibsxx ${RUMPSRC})"

	userincludes ${RUMPSRC} ${LIBS} $(pwd)/lib/librumprun_tester
	for lib in ${LIBS}; do
		makeuserlib ${lib}
	done
	makeuserlib $(pwd)/lib/librumprun_tester ${PLATFORM}

	# build unwind bits if we support c++
	if ${HAVECXX}; then
		( cd lib/libunwind
		    ${RUMPMAKE} ${STDJ} obj
		    ${RUMPMAKE} ${STDJ} includes
		    ${RUMPMAKE} ${STDJ} dependall
		    ${RUMPMAKE} ${STDJ} install
		)
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

buildkernlibs ()
{

	( cd lib/librumpkern_bmktc
		${RUMPMAKE} ${STDJ} obj
		${RUMPMAKE} ${STDJ} dependall
		${RUMPMAKE} ${STDJ} install
	)
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
	echo "MACHINE_ARCH=${MACHINE_ARCH}" >> ${1}
	echo "KERNONLY=${KERNONLY}" >> ${1}

	echo "DESTDIR=${DESTDIR}" >> ${1}
	echo "OBJDIR=${RROBJ}" >> ${1}

	# wrap mandatory toolchain bits
	for t in AR AS CC CPP LD NM OBJCOPY OBJDUMP RANLIB READELF \
            SIZE STRINGS STRIP; do
		wraponetool ${1} ${t}
	done

	# c++ is optional, wrap it iff available
	if ${HAVECXX}; then
		echo "CONFIG_CXX=yes" >> ${1}
		wraponetool ${1} CXX
	else
		echo "CONFIG_CXX=no" >> ${1}
	fi
}

dobuild ()
{

	checksubmodules

	. ${BUILDRUMP}/subr.sh

	PLATFORM_MKCONF=
	. ${PLATFORMDIR}/platform.conf

	buildrump "$@"
	${KERNONLY} || builduserspace

	# depends on config.mk
	buildpci

	buildkernlibs

	# run routine specified in platform.conf
	doextras || die 'platforms extras failed.  tillerman needs tea?'

	# do final build of the platform bits
	( cd ${PLATFORMDIR} && ${MAKE} BUILDRR=true || exit 1)
	[ $? -eq 0 ] || die platform make failed!
}

doinstall ()
{

	setvars

	# default used to be a symlink, so this is for "compat".
	# remove in a few months.
	rm -f ${DESTDIR} > /dev/null 2>&1

	mkdir -p ${DESTDIR} || die cannot create ${DESTDIR}
	( cd ${STAGING} ; tar -cf - .) | (cd ${DESTDIR} ; tar -xf -)
}

#
# BEGIN SCRIPT
#

parseargs "$@"
shift ${ARGSSHIFT}

${DObuild} && dobuild "$@"
${DOinstall} && doinstall

# echo some useful information for the user
echo
echo '>>'
echo ">> Finished $0 for ${PLATFORM}"
if ${DObuild}; then
	printf ">> ${TOOLTUPLE}"
	printf ">> cc: %s-%s\n", \
	   ${TOOLTUPLE} "$(${RUMPMAKE} -f bsd.own.mk -V '${ACTIVE_CC}')"
fi
if ${DOinstall}; then
	printf ">> installed to \"%s\"\n" ${DESTDIR}
fi
echo '>>'
echo ">> $0 ran successfully"
exit 0
