#!/bin/sh

# just a script to run the handful of commands required for a
# bootable domU image

# the buildxen.sh is not as forgiving as I am
set -e

# fetch buildrump.sh and NetBSD sources
git submodule update --init --recursive
./buildrump.sh/buildrump.sh -q -k -s rumpsrc -T rumptools -o rumpobj checkout

# hackish, but meh, we want to get rid of this anyway
(
  cd rumpsrc
  if [ ! -d xen-nblibc ] ; then
    mv lib/libc lib/libc.orig
    git clone https://github.com/anttikantee/xen-nblibc
    ln -s ../xen-nblibc/libc lib/libc
    ln -s ../xen-nblibc/libpthread lib/libpthread
  else
    ( cd xen-nblibc && git pull )
  fi
)

# build rump kernel
./buildrump.sh/buildrump.sh -q -k -s rumpsrc -T rumptools -o rumpobj \
    tools build install

#
# install full set of headers.
#
# first, "mtree" (TODO: fetch/use nbmtree)
INCSDIRS='adosfs altq arpa crypto dev filecorefs fs i386 isofs miscfs
	msdosfs net net80211 netatalk netbt netinet netinet6 netipsec
	netisdn netkey netmpls netnatm netsmb nfs ntfs ppath prop
	protocols rpc rpcsvc ssp sys ufs uvm x86'
for dir in ${INCSDIRS}; do
	mkdir -p rump/include/$dir
done

# then, install
echo '>> Installing headers.  please wait (may take a while) ...'
(
  # sys/ produces a lot of errors due to missing tools/sources
  # "protect" the user from that spew
  cd rumpsrc/sys
  ../../rumptools/rumpmake -k includes >/dev/null 2>&1
)

# rpcgen lossage
( cd rumpsrc/include && ../../rumptools/rumpmake -k includes > /dev/null 2>&1)

( cd rumpsrc/lib/libc && ../../../rumptools/rumpmake includes >/dev/null 2>&1)
( cd rumpsrc/lib/libpthread && ../../../rumptools/rumpmake includes >/dev/null)


# build networking driver
(
  cd rumpxenif
  ../rumptools/rumpmake dependall MKPIC=no && ../rumptools/rumpmake install
)

# build & install libc
(
  cd rumpsrc/lib/libc
  ../../../rumptools/rumpmake MKMAN=no MKLINT=no MKPIC=no MKPROFILE=no MKYP=no \
    dependall
  ../../../rumptools/rumpmake MKMAN=no MKLINT=no MKPIC=no MKPROFILE=no MKYP=no \
    install
)

[ ! -f test.ffs ] && cp test_clean.ffs test.ffs

# build the domU image
make
