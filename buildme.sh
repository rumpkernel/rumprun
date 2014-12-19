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
    -V RUMP_KERNEL_IS_LIBC=1 "$@" tools

# COMMON SCRIPT would be special mk.conf variables here.  we don't
:

RUMPMAKE=$(pwd)/rumptools/rumpmake
MACHINE=$(${RUMPMAKE} -f /dev/null -V '${MACHINE}')
[ -z "${MACHINE}" ] && die could not figure out target machine

# build rump kernel
./buildrump.sh/buildrump.sh ${BUILDXEN_QUIET} ${STDJ} -k \
    -V MKPIC=no -s ${RUMPSRC} -T rumptools -o rumpobj -N \
    -V RUMP_KERNEL_IS_LIBC=1 "$@" build kernelheaders install

LIBS="$(stdlibs ${RUMPSRC})"
usermtree rump
userincludes ${RUMPSRC} ${LIBS}

for lib in ${LIBS}; do
	makeuserlib ${lib}
done

# build PCI drivers only on x86 (needs MD support)
[ "${MACHINE}" = 'amd64' -o "${MACHINE}" = 'i386' ] \
    && makepci ${RUMPSRC} RUMP_PCI_IOSPACE=yes

# build the domU image
make

echo
echo ">> $0 ran successfully"
exit 0
