#!/bin/sh

# Just a script to run the handful of commands required for a
# bootable domU image.  This is mostly to document the commands
# required, and is not pretending to be fancy.

STDJ='-j4'
RUMPSRC=rumpsrc

while getopts '?qs:' opt; do
	case "$opt" in
	's')
		RUMPSRC=${OPTARG}
		;;
	'q')
		BUILDXEN_QUIET=${BUILDXEN_QUIET:=-}q
		;;
	'?')
		exit 1
	esac
done
shift $((${OPTIND} - 1))

# the buildxen.sh is not as forgiving as I am
set -e

[ ! -f ./buildrump.sh/subr.sh ] && git submodule update --init buildrump.sh
. ./buildrump.sh/subr.sh

if git submodule status ${RUMPSRC} | grep -q '^-' ; then
	git submodule update --init --recursive ${RUMPSRC}
fi
[ "$1" = "justcheckout" ] && { echo ">> $0 done" ; exit 0; }

# build tools
./buildrump.sh/buildrump.sh ${BUILDXEN_QUIET} ${STDJ} -k \
    -V MKPIC=no -s ${RUMPSRC} -T rumptools -o rumpobj -N \
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
./buildrump.sh/buildrump.sh -k -V MKPIC=no -s ${RUMPSRC} -T rumptools -o rumpobj build kernelheaders install

LIBS="$(stdlibs ${RUMPSRC})"
usermtree rump
userincludes ${RUMPSRC} ${LIBS}

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
makepci ${RUMPSRC}

for lib in ${LIBS}; do
	makeuserlib ${lib}
done

./buildrump.sh/buildrump.sh ${BUILDXEN_QUIET} $* \
    -s ${RUMPSRC} -T rumptools -o rumpobj install

[ ! -f img/test.ffs ] && cp img/test_clean.ffs img/test.ffs

# build the domU image
make

echo
echo ">> $0 ran successfully"
exit 0
