#!/bin/sh

# Script to build "userspace".  Note, not "consumer-grade" yet!
#
# first run buildrump.sh with:
#   ./buildrump.sh -s ${APPSTACK_SRC} -V MKPIC=no -V RUMP_KERNEL_IS_LIBC=1 \
#      -k -N kernelheaders fullbuild
#
# NOTE: if you're building on x86_64, add -F ACLFLAGS=-m32 to the above
# see also .travis.yml for more "documentation"
#

set -e

: ${APPSTACK_SRC:=/home/pooka/src/src-netbsd-appstack}
: ${BUILDRUMP_SH:=/home/pooka/src/buildrump.sh}

STDJ=-j4

RUMPMAKE=${BUILDRUMP_SH}/obj/tooldir/rumpmake

MACHINE=$(${RUMPMAKE} -f /dev/null -V '${MACHINE}')
[ -z "${MACHINE}" ] && { echo 'could not figure out target machine'; exit 1; }

. ${BUILDRUMP_SH}/subr.sh

LIBS="$(stdlibs ${APPSTACK_SRC})"
usermtree ${BUILDRUMP_SH}/rump
userincludes ${RUMPMAKE} ${APPSTACK_SRC} ${LIBS}

for lib in ${LIBS}; do
	unset extraarg
	# force not building c++ unwinding for arm
	[ "$(basename ${lib})" = "libc" -a "${MACHINE}" = "arm" ] \
	    && extraarg='HAVE_LIBGCC_EH=yes'
        makeuserlib ${RUMPMAKE} ${lib} ${extraarg}
done

# build PCI drivers only on x86 (needs MD support)
[ "${MACHINE}" = 'amd64' -o "${MACHINE}" = 'i386' ] && \
    APPSTACK_LIBS=$(${RUMPMAKE} \
      -f ${APPSTACK_SRC}/sys/rump/dev/Makefile.rumpdevcomp -V '${RUMPPCIDEVS}')

for lib in ${APPSTACK_LIBS}; do
		( cd ${APPSTACK_SRC}/sys/rump/dev/lib/lib${lib}
			${RUMPMAKE} obj
			${RUMPMAKE} RUMP_PCI_IOSPACE=yes dependall
			${RUMPMAKE} install
		)
done

export BUILDRUMP_SH
make
