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

: ${BUILDRUMP_SH:=/home/pooka/src/buildrump.sh}
. ${BUILDRUMP_SH}/subr.sh

[ -z "${APPSTACK_SRC}" ] || die 'setting $APPSTACK_SRC is no longer allowed'

STDJ=-j4

RUMPMAKE=${BUILDRUMP_SH}/obj/tooldir/rumpmake
APPSTACK_SRC=$(${RUMPMAKE} -f /dev/null -V '${NETBSDSRCDIR}')

MACHINE=$(${RUMPMAKE} -f /dev/null -V '${MACHINE}')
[ -z "${MACHINE}" ] && die could not figure out target machine

LIBS="$(stdlibs ${APPSTACK_SRC})"
usermtree ${BUILDRUMP_SH}/rump
userincludes ${APPSTACK_SRC} ${LIBS}

for lib in ${LIBS}; do
	unset extraarg
	# force not building c++ unwinding for arm
	[ "$(basename ${lib})" = "libc" -a "${MACHINE}" = "arm" ] \
	    && extraarg='HAVE_LIBGCC_EH=yes'
        makeuserlib ${lib} ${extraarg}
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
