#!/bin/sh

#
# Copyright (c) 2015 Antti Kantee <pooka@rumpkernel.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# TODO: use a more scalable way of specifying tests
TESTS='hello/hello basic/ctor_test basic/pthread_test basic/tls_test
	crypto/md5'

STARTMAGIC='=== FOE RUMPRUN 12345 TES-TER 54321 ==='
ENDMAGIC='=== RUMPRUN 12345 TES-TER 54321 EOF ==='

OPT_SUDO=

die ()
{

	echo '>> ERROR:'
	echo ">> $*"
	exit 1
}

ddimage ()
{

	imgname=$1
	imgsize=$2
	shift 2 || die not enough arguments to \"ddimage\"
	imgsource=${3:-/dev/zero}

	blocks=$((${imgsize}/512))
	[ ${imgsize} -eq $((${blocks}*512)) ] \
	    || die imgsize \"${imgsize}\" not 512-byte aligned

	dd if=${imgsource} of=${imgname} bs=512 count=${blocks} > /dev/null 2>&1
}

runguest ()
{

	testprog=$1
	img1=$2
	# notyet
	# img2=$3

	[ -n "${img1}" ] || die runtest without a disk image
	cookie=$(${RUMPRUN} ${STACK} ${OPT_SUDO} -b ${img1} ${testprog} __test)
	if [ $? -ne 0 -o -z "${cookie}" ]; then
		TEST_RESULT=ERROR
		TEST_ECODE=-2
	else
		TEST_RESULT=TIMEOUT
		TEST_ECODE=-1

		for x in $(seq 10) ; do
			echo ">> polling, round ${x} ..."
			set -- $(sed 1q < ${img1})

			case ${1} in
			OK)
				TEST_RESULT=SUCCESS
				TEST_ECODE=$2
				break
				;;
			NO)
				TEST_RESULT=FAILED
				TEST_ECODE=$2
				break
				;;
			*)
				# continue
				;;
			esac

			sleep 1
		done

		${RUMPSTOP} ${OPT_SUDO} ${cookie}
	fi

	echo ">> Result: ${TEST_RESULT} (${TEST_ECODE})"
}

getoutput ()
{

	img=${1}
	shift || die 'getoutput: not enough args'
	sed -n "/${STARTMAGIC}/,/${ENDMAGIC}/p" < ${img} | sed -n '1n;$n;p'
}

runtest ()
{

	bin=$1

	ddimage disk.img 1024
	runtest tester disk.img
}

cd $(dirname $0) || die 'could not enter test dir'
RUMPRUN=$(pwd)/../app-tools/rumprun
RUMPSTOP=$(pwd)/../app-tools/rumpstop

[ $# -ge 1 ] || die "usage: runtests.sh [-S] qemu|xen"

if [ "$1" = '-S' ]; then
	shift
	[ $(id -u) -ne 0 ] && OPT_SUDO=-S
fi

STACK=$1
[ ${STACK} != none ] || exit 0

TESTDIR=$(mktemp -d testrun.XXXXXX)
[ $? -eq 0 ] || die failed to create datadir for testrun

TOPDIR=$(pwd)
cd ${TESTDIR}

rv=0
for test in ${TESTS}; do
	echo ">> Running test: ${test}"

	testunder="$(echo ${test} | sed s,/,_,g)"
	outputimg=${testunder}.disk1

	ddimage ${outputimg} $((2*512))
	runguest ${TOPDIR}/${test} ${outputimg}

	echo ">> Test output for ${test}"
	getoutput ${outputimg}
	echo ">> End test outout"

	echo ${test} ${TEST_RESULT} ${TEST_ECODE} >> test.log
	[ "${TEST_RESULT}" != 'SUCCESS' ] && rv=1
	echo
done

echo '>> TEST LOG'
cat test.log

awk '{res[$2]++}
    END{printf "Success: %d, Fail: %d, Timeout: %d, Internal error: %d\n",
        res["SUCCESS"], res["FAILED"], res["TIMEOUT"], res["ERROR"]}' < test.log

exit ${rv}
