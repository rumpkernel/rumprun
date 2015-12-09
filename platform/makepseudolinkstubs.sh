#!/bin/sh

makealias ()
{

	printf "\n" >> ${OUTPUT}
	printf "__weak_alias(%s,_stubnosys);\n" $1 >> ${OUTPUT}
	printf "__weak_alias(_%s,_stubnosys);\n" $1 >> ${OUTPUT}
	printf "__strong_alias(_sys_%s,_stubnosys);\n" $1 >> ${OUTPUT}
}

[ $# -eq 4 ] || { echo invalid usage; exit 1;}

NM=$1
RUMPSRC=$2
BASELIB=$3
OUTPUT=$4

rm -f ${OUTPUT}
printf '/* AUTOMATICALLY GENERATED */\n\n' >> ${OUTPUT}
printf '#include <sys/cdefs.h>\n\n' >> ${OUTPUT}
printf 'extern int main(int, char**);\n' >> ${OUTPUT}
printf 'int _want_main(void); int _want_main(void) {return main(0, 0);}\n' \
    >> ${OUTPUT}
printf 'int _stubnosys(void); int _stubnosys(void) {return -1;}\n' \
    >> ${OUTPUT}

# special cases
printf "__strong_alias(rump_syscall,_stubnosys);\n" >> ${OUTPUT}
printf "__strong_alias(_start,_stubnosys);\n\n" >> ${OUTPUT}

# symbols not convered by libc
${NM} -o -g --defined-only ${BASELIB} | awk '
$(NF-1) == "T" {
	printf("__strong_alias(%s,_stubnosys);\n", $NF);
	next
}
$(NF-1) == "W" {
	printf("__weak_alias(%s,_stubnosys);\n", $NF);
	next
}
$(NF-1) ~ "(D|C)" {
	printf("int %s;\n", $NF);
	next
}
{
	printf("error, symbol type %s not handled\n", $(NF-1)) | "cat 1>&2";
	exit(1)
}
' >> ${OUTPUT} || exit 1

# system calls
awk '{print $3}' < ${RUMPSRC}/sys/rump/rump.sysmap | while read syscall; do
	makealias ${syscall}
done

exit 0
