MYDIR:=	${.PARSEDIR}
.PATH:	${MYDIR}

SRCS+=	cpu_sched_switch.S

.include "${MYDIR}/../x86/Makefile.inc"
