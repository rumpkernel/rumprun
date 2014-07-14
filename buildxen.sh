#!/bin/sh

# Just a script to run the handful of commands required for a
# bootable domU image.  This is mostly to document the commands
# required, and is not pretending to be fancy.

STDJ='-j4'

# the buildxen.sh is not as forgiving as I am
set -e

[ ! -f ./buildrump.sh/subr.sh ] && git submodule update --init buildrump.sh
. ./buildrump.sh/subr.sh

if git submodule status rumpsrc | grep -q '^-' ; then
	git submodule update --init --recursive rumpsrc
fi
[ "$1" = "justcheckout" ] && { echo ">> $0 done" ; exit 0; }

MORELIBS="external/bsd/flex/lib
	crypto/external/bsd/openssl/lib/libcrypto
	crypto/external/bsd/openssl/lib/libdes
	crypto/external/bsd/openssl/lib/libssl
	external/bsd/libpcap/lib"
LIBS="rumpsrc/lib/lib*"
for lib in ${MORELIBS}; do
	LIBS="${LIBS} rumpsrc/${lib}"
done

# build tools
./buildrump.sh/buildrump.sh -${BUILDXEN_QUIET:-q} ${STDJ} -k \
    -V MKPIC=no -s rumpsrc -T rumptools -o rumpobj -N \
    -V RUMP_CURLWP=hypercall -V RUMP_KERNEL_IS_LIBC=1 tools
# FIXME to be able to specify this as part of previous cmdline
echo 'CPPFLAGS+=-DMAXPHYS=32768' >> rumptools/mk.conf

# set some special variables currently required by libpthread.  Doing
# it this way preserves the ability to compile libpthread during development
# cycles with just "rumpmake"
cat >> rumptools/mk.conf << EOF
.if defined(LIB) && \${LIB} == "pthread"
PTHREAD_CANCELSTUB=no
CPPFLAGS+=      -D_PLATFORM_MAKECONTEXT=_lwp_rumpxen_makecontext
CPPFLAGS+=      -D_PLATFORM_GETTCB=_lwp_rumpxen_gettcb
.endif  # LIB == pthread
EOF

RUMPMAKE=$(pwd)/rumptools/rumpmake

# build rump kernel
./buildrump.sh/buildrump.sh -k -V MKPIC=no -s rumpsrc -T rumptools -o rumpobj build kernelheaders install

usermtree rump
userincludes ${RUMPMAKE} rumpsrc ${LIBS}

make links

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
makekernlib rumpxendev

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
