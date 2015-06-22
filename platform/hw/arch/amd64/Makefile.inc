OBJS_BMK+=	arch/amd64/locore.o arch/amd64/intr.o
OBJS_BMK+=	arch/amd64/machdep.o arch/amd64/boot.o

OBJS_BMK+=	arch/x86/vgacons.o
OBJS_BMK+=	arch/x86/cpu_subr.o
OBJS_BMK+=	arch/x86/x86_subr.o
OBJS_BMK+=	arch/x86/clock.o

LDSCRIPT=	arch/amd64/kern.ldscript

CFLAGS+=	-mno-sse -mno-mmx

# squash all interrupts to a single level to
# avoid having to figure out routing
CFLAGS+=	-DBMK_SCREW_INTERRUPT_ROUTING

LDFLAGS_BAKE+=	-z max-page-size=0x1000

arch/amd64/locore.o: arch/amd64/locore.S
	${CC} ${CFLAGS} ${CPPFLAGS} -D_LOCORE -c -o arch/amd64/locore.o $<

arch/amd64/intr.o: arch/amd64/intr.S
	${CC} ${CFLAGS} ${CPPFLAGS} -D_LOCORE -c -o arch/amd64/intr.o $<
