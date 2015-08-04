MYDIR:=	${.PARSEDIR}
.PATH:	${MYDIR}

SRCS+=	cpu_sched_switch.S __aeabi_read_tp.S
SRCS+=	cpu_sched.c
