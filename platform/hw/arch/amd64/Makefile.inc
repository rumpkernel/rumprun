OBJS_BMK+=	arch/amd64/locore.o arch/amd64/intr.o
OBJS_BMK+=	arch/amd64/machdep.o

OBJS_BMK+=	arch/x86/boot.o
OBJS_BMK+=	arch/x86/cons.o arch/x86/vgacons.o arch/x86/serialcons.o
OBJS_BMK+=	arch/x86/cpu_subr.o
OBJS_BMK+=	arch/x86/x86_subr.o
OBJS_BMK+=	arch/x86/clock.o
OBJS_BMK+=      arch/x86/hypervisor.o

CFLAGS+=	-mno-sse -mno-mmx

LDFLAGS_BAKE+= -z max-page-size=0x1000

# squash all interrupts to a single level to
# avoid having to figure out routing
CFLAGS+=	-DBMK_SCREW_INTERRUPT_ROUTING

arch/amd64/locore.o: arch/amd64/locore.S
	${CC} ${CFLAGS} ${CPPFLAGS} -D_LOCORE -c -o arch/amd64/locore.o $<

arch/amd64/intr.o: arch/amd64/intr.S
	${CC} ${CFLAGS} ${CPPFLAGS} -D_LOCORE -c -o arch/amd64/intr.o $<
