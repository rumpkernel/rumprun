include ../../global.mk
include ../../platform/hw/config.mk

ifdef BUILDRUMP_TOOLFLAGS
include ${BUILDRUMP_TOOLFLAGS}
endif

CFLAGS+=	${BUILDRUMP_TOOL_CFLAGS}

LDFLAGS:= -L$(abspath ../../rumprun/lib)
LDFLAGS+= -L$(abspath ../../lib/libbmk_core/hw/)
LDFLAGS+= -L$(abspath ../../lib/libbmk_rumpuser/hw/)
LDFLAGS+= -L$(abspath ../../lib/libcompiler_rt/hw)

CPPFLAGS+= -I../../include -I../../rumprun/include -I../../platform/hw/include
CPPFLAGS+= -nostdlib

LDSCRIPT=$(abspath ../../platform/hw/arch/${MACHINE}/kern.ldscript)

ifeq (${MACHINE},amd64)
LDFLAGS+= -z max-page-size=0x1000
endif

ifeq (${MACHINE},i386)
LDFLAGS+= -m elf_i386
endif

OBJS= main.o $(abspath ../../platform/hw/rumprun-hw-${MACHINE_ARCH}.o)

.PHONY: clean

main.elf: ${OBJS}
	${LD} ${LDFLAGS} -T${LDSCRIPT} 					\
	${OBJS} 							\
	--whole-archive -lrumpvfs -lrump --no-whole-archive		\
	-lbmk_core -lbmk_rumpuser					\
	-lcompiler_rt 							\
	-o $@

clean:
	rm -rf main.o main.elf
