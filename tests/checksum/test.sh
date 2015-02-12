#!/bin/bash
# ^ bash for job control

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
	fi
	sleep 1
done

# cleanup
kill %1
rm disk.img
exit ${rv}
