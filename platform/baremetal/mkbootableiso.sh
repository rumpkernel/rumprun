[ $# -eq 0 ] && { echo give kernel name! ; exit 1 ; }

basedir="/tmp/mkbootable$$"

mkdir -p ${basedir}/boot/grub
printf 'menuentry "rumpkernel" {\n\tmultiboot /boot/%s\n}\n' $1 \
    > ${basedir}/boot/grub/grub.cfg
ln -f $1 ${basedir}/boot
grub-mkrescue -o $1.iso ${basedir}
