#!/bin/sh

export BUILDXENMETAL_PCI_P='[ "${MACHINE}" = "amd64" -o "${MACHINE}" = "i386" ]'
export BUILDXENMETAL_PCI_ARGS='RUMP_PCI_IOSPACE=yes'

[ ! -f ./buildrump.sh/subr.sh ] && git submodule update --init buildrump.sh
. ./buildrump.sh/subr.sh
./buildrump.sh/xenbaremetal.sh "$@" || die xenbaremetal.sh failed

# build the image
make

echo
echo ">> $0 ran successfully"
exit 0
