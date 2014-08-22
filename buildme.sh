#!/bin/sh

# Script to build "userspace".  Note, not "consumer-grade" yet!
#
# first run buildrump.sh with:
#   ./buildrump.sh -s ${APPSTACK_SRC} -V MKPIC=no -V RUMP_KERNEL_IS_LIBC=1 \
#      -V RUMP_CURLWP=hypercall -k -N -H kernelheaders fullbuild

: ${APPSTACK_SRC:=/home/pooka/src/src-netbsd-appstack}
: ${BUILDRUMP_SH:=/home/pooka/src/buildrump.sh}

MORELIBS="external/bsd/flex/lib
        crypto/external/bsd/openssl/lib/libcrypto
        crypto/external/bsd/openssl/lib/libdes
        crypto/external/bsd/openssl/lib/libssl
        external/bsd/libpcap/lib"
LIBS="${APPSTACK_SRC}/lib/lib*"
for lib in ${MORELIBS}; do
        LIBS="${LIBS} ${APPSTACK_SRC}/${lib}"
done

RUMPMAKE=${BUILDRUMP_SH}/obj/tooldir/rumpmake

. ${BUILDRUMP_SH}/subr.sh

usermtree ${BUILDRUMP_SH}/rump
userincludes ${RUMPMAKE} ${APPSTACK_SRC} ${LIBS}

for lib in ${LIBS}; do
        makeuserlib ${RUMPMAKE} ${lib}
done

APPSTACK_LIBS=$(${RUMPMAKE} -f ${APPSTACK_SRC}/sys/rump/dev/Makefile.rumpdevcomp -V '${RUMPPCIDEVS}')

for lib in ${APPSTACK_LIBS}; do
		( cd ${APPSTACK_SRC}/sys/rump/dev/lib/lib${lib}
			${RUMPMAKE} obj
			${RUMPMAKE} RUMP_PCI_IOSPACE=yes dependall
			${RUMPMAKE} install
		)
done
