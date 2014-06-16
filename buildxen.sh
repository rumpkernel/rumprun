#!/bin/sh

# Just a script to run the handful of commands required for a
# bootable domU image.  This is mostly to document the commands
# required, and is not pretending to be fancy.

STDJ='-j4'

# the buildxen.sh is not as forgiving as I am
set -e

LIBLIBS="c crypt ipsec m npf pthread prop rmt util pci y z"
MORELIBS="external/bsd/flex/lib
	crypto/external/bsd/openssl/lib/libcrypto
	crypto/external/bsd/openssl/lib/libdes
	crypto/external/bsd/openssl/lib/libssl
	external/bsd/libpcap/lib"
LIBS=""
for lib in ${LIBLIBS}; do
	LIBS="${LIBS} rumpsrc/lib/lib${lib}"
done
for lib in ${MORELIBS}; do
	LIBS="${LIBS} rumpsrc/${lib}"
done

# ok, urgh, we need just one tree due to how build systems work (or
# don't work).  So here's what we'll do for now.  Checkout rumpsrc,
# checkout nbusersrc, and copy nbusersrc over rumpsrc.  Obviously, we cannot
# update rumpsrc except manually after the copy operation, but that's
# a price we're just going to be paying for now.
if [ ! -d rumpsrc ]; then
	git submodule update --init --recursive
	./buildrump.sh/buildrump.sh -s rumpsrc checkout
	cp -Rp nblibs/* rumpsrc/
fi

[ "$1" = "justcheckout" ] && { echo ">> $0 done" ; exit 0; }

# build tools
./buildrump.sh/buildrump.sh -${BUILDXEN_QUIET:-q} ${STDJ} -k \
    -V MKPIC=no -s rumpsrc -T rumptools -o rumpobj -N \
    -V RUMP_CURLWP=hypercall -V RUMP_KERNEL_IS_LIBC=1 tools
# FIXME to be able to specify this as part of previous cmdline
echo 'CPPFLAGS+=-DMAXPHYS=32768' >> rumptools/mk.conf

RMAKE=`pwd`/rumptools/rumpmake
RMAKE_INST=`pwd`/rumptools/_buildrumpsh-rumpmake

# build rump kernel
./buildrump.sh/buildrump.sh -k -V MKPIC=no -s rumpsrc -T rumptools -o rumpobj build kernelheaders install

# install full set of userspace
#
# first, "mtree" (TODO: fetch/use nbmtree)
INCSDIRS='adosfs altq arpa crypto dev filecorefs fs i386 isofs miscfs
	msdosfs net net80211 netatalk netbt netinet netinet6 netipsec
	netisdn netkey netmpls netnatm netsmb nfs ntfs openssl pcap ppath prop
	protocols rpc rpcsvc ssp sys ufs uvm x86'
for dir in ${INCSDIRS}; do
	mkdir -p rump/include/$dir
done
# XXX
mkdir -p rumpobj/dest.stage/usr/lib/pkgconfig

echo '>> installing userspace headers'
( cd rumpsrc/include && ${RMAKE} includes )
for lib in ${LIBS}; do
	( cd ${lib} && ${RMAKE} includes )
done
echo '>> done with headers'

makekernlib ()
{
	lib=$1
	OBJS=`pwd`/rumpobj/$lib
	mkdir -p ${OBJS}
	( cd ${lib}
		${RMAKE} MAKEOBJDIRPREFIX=${OBJS} obj
		${RMAKE} MAKEOBJDIRPREFIX=${OBJS} dependall
		${RMAKE} MAKEOBJDIRPREFIX=${OBJS} install
	)
}
makekernlib rumpxenif
makekernlib rumpxenpci

makeuserlib ()
{

	( cd $1
		${RMAKE} obj
		${RMAKE} MKMAN=no MKLINT=no MKPROFILE=no MKYP=no \
		    NOGCCERROR=1 ${STDJ} dependall
		${RMAKE_INST} MKMAN=no MKLINT=no MKPROFILE=no MKYP=no install
	)
}
for lib in ${LIBS}; do
	makeuserlib ${lib}
done

./buildrump.sh/buildrump.sh ${BUILD_QUIET} $* \
    -s rumpsrc -T rumptools -o rumpobj install

[ ! -f img/test.ffs ] && cp img/test_clean.ffs img/test.ffs

# build httpd objects
( cd httpd && make -f Makefile.boot )

# build the domU image
make
