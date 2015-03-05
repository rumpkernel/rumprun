#!/bin/sh

#
# simple script which can be run with constant parameters to invoke gdbsx
#

[ $(id -u) = 0 ] || { echo must be root ; exit 1; }

case `uname -m` in
i*86)
	bits=32
	;;
i86pc)
	bits=32
	;;
*)
	bits=64
	;;
esac
domid=$(xl list | awk '$1 == "'${1:-rump-kernel}'"{print $2}')

gdbsx -a ${domid} ${bits} ${2:-1234}
