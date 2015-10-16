PLATFORM?= hw

include ../../global.mk
-include ../../platform/${PLATFORM}/config.mk

ifdef BUILDRUMP_TOOLFLAGS
include ${BUILDRUMP_TOOLFLAGS}
endif

CFLAGS+=	${BUILDRUMP_TOOL_CFLAGS}

LDFLAGS:= -L$(abspath ../../rumprun/lib)
LDFLAGS+= -L$(abspath ../../lib/libcompiler_rt/${PLATFORM})

CPPFLAGS+= -I../../include -I../../rumprun/include -I../../platform/${PLATFORM}/include

LDSCRIPT=$(abspath ../../platform/${PLATFORM}/bmk.ldscript)

LDFLAGS+= ${LDFLAGS.${PLATFORM}.${MACHINE_ARCH}}

OBJS= main.o $(abspath ../../platform/${PLATFORM}/rumprun-${PLATFORM}-${MACHINE_ARCH}.o)

.PHONY: clean

main.elf: ${OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} -T${LDSCRIPT} 			\
	${OBJS} 							\
	-nostdlib							\
	-Wl,--whole-archive -lrumpvfs -lrump -Wl,--no-whole-archive	\
	-lcompiler_rt 							\
	-o $@

clean:
	rm -rf main.o main.elf
