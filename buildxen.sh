#!/bin/sh

# Just a script to run the handful of commands required for a
# bootable domU image.  This is mostly to document the commands
# required, and is not pretending to be fancy.

STDJ='-j4'

# the buildxen.sh is not as forgiving as I am
set -e
. ./buildrump.sh/subr.sh

MORELIBS="external/bsd/flex/lib
	crypto/external/bsd/openssl/lib/libcrypto
	crypto/external/bsd/openssl/lib/libdes
	crypto/external/bsd/openssl/lib/libssl
	external/bsd/libpcap/lib"
LIBS="$(echo nblibs/lib/lib* | sed 's/nblibs/rumpsrc/g')"
for lib in ${MORELIBS}; do
	LIBS="${LIBS} rumpsrc/${lib}"
done

docheckout rumpsrc nblibs

[ "$1" = "justcheckout" ] && { echo ">> $0 done" ; exit 0; }

# build tools
./buildrump.sh/buildrump.sh -${BUILDXEN_QUIET:-q} ${STDJ} -k \
    -V MKPIC=no -s rumpsrc -T rumptools -o rumpobj -N \
    -V RUMP_CURLWP=hypercall -V RUMP_KERNEL_IS_LIBC=1 tools
# FIXME to be able to specify this as part of previous cmdline
echo 'CPPFLAGS+=-DMAXPHYS=32768' >> rumptools/mk.conf

RUMPMAKE=$(pwd)/rumptools/rumpmake

# build rump kernel
./buildrump.sh/buildrump.sh -k -V MKPIC=no -s rumpsrc -T rumptools -o rumpobj build kernelheaders install

usermtree
userincludes ${RUMPMAKE} rumpsrc ${LIBS}

makekernlib ()
{
	lib=$1
	OBJS=`pwd`/rumpobj/$lib
	mkdir -p ${OBJS}
	( cd ${lib}
		${RUMPMAKE} MAKEOBJDIRPREFIX=${OBJS} obj
		${RUMPMAKE} MAKEOBJDIRPREFIX=${OBJS} dependall
		${RUMPMAKE} MAKEOBJDIRPREFIX=${OBJS} install
	)
}
makekernlib rumpxenif
makekernlib rumpxenpci

for lib in ${LIBS}; do
	makeuserlib ${RUMPMAKE} ${lib}
done

./buildrump.sh/buildrump.sh ${BUILD_QUIET} $* \
    -s rumpsrc -T rumptools -o rumpobj install

[ ! -f img/test.ffs ] && cp img/test_clean.ffs img/test.ffs

# build httpd objects
( cd httpd && make -f Makefile.boot )

# build the domU image
make
