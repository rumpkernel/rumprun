#!/bin/bash
# ^ bash for job control

if ! type qemu-system-i386 >/dev/null 2>&1; then
	echo ERROR: qemu-system-i386 required but not found
	exit 1
fi

dd if=/dev/urandom of=disk.img count=1 2>/dev/null
echo 'PLEASE WRITE ON THIS IMAGE' | dd of=disk.img conv=notrunc 2>/dev/null

sum=$(md5sum disk.img | awk '{print $1}')
qemu-system-i386 -net none -drive if=virtio,file=disk.img -display none	\
    -no-kvm -kernel test-app &

rv=1

# poll for results
for x in $(seq 10) ; do
	echo 'polling ...'
	res=$(sed q disk.img)
	if [ "${res}" = "OK" ]; then
		emusum=$(sed -n '2{p;q;}' disk.img)
		echo res ok
		echo sum1: ${sum}
		echo sum2: ${emusum}
		if [ "${sum}" = "${emusum}" ]; then
			rv=0
		fi
		break
	elif [ "${res}" = "ERROR" ]; then
		emesg=$(sed -n '2{p;q;}' disk.img)
		echo GOT ERROR: ${emesg}
		rv=1
		break
	fi
	sleep 1
done

# cleanup
kill %1
rm disk.img
exit ${rv}
