#!/bin/sh

# Script to build "userspace".  Note, not "consumer-grade" yet!
#
# first run buildrump.sh with:
# ./buildrump.sh -s ${APPSTACK_SRC} -V MKPIC=no -k -N -H kernelheaders fullbuild

APPSTACK_SRC= /home/pooka/src/src-netbsd-appstack
BUILDRUMP_SH= /home/pooka/src/buildrump.sh

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

