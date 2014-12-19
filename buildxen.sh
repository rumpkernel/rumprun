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

# build tools
./buildrump.sh/buildrump.sh -${BUILDXEN_QUIET:-q} ${STDJ} -k \
    -V MKPIC=no -s rumpsrc -T rumptools -o rumpobj -N \
    -V RUMP_KERNEL_IS_LIBC=1 tools

# set some special variables.
cat >> rumptools/mk.conf << EOF
# maxphys = 32k is a Xen limitation (64k - overhead)
CPPFLAGS+=-DMAXPHYS=32768
.if defined(LIB) && \${LIB} == "pthread"
.PATH:	$(pwd)
PTHREAD_MAKELWP=pthread_makelwp_rumprunxen.c
CPPFLAGS+=      -D_PTHREAD_GETTCB_EXT=_lwp_rumpxen_gettcb
.endif  # LIB == pthread
EOF

RUMPMAKE=$(pwd)/rumptools/rumpmake

# build rump kernel
./buildrump.sh/buildrump.sh -k -V MKPIC=no -s rumpsrc -T rumptools -o rumpobj build kernelheaders install

LIBS="$(stdlibs rumpsrc)"
usermtree rump
userincludes rumpsrc ${LIBS}

make -C xen links

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
makekernlib rumpxendev
makepci rumpsrc

for lib in ${LIBS}; do
	makeuserlib ${lib}
done

./buildrump.sh/buildrump.sh ${BUILD_QUIET} $* \
    -s rumpsrc -T rumptools -o rumpobj install

[ ! -f img/test.ffs ] && cp img/test_clean.ffs img/test.ffs

# build the domU image
make

echo
echo ">> $0 ran successfully"
exit 0
