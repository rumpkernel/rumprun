PLATFORM=hw
PLATFORM_DEFAULT_TESTER=qemu
OBJ_DIR=.

all: app-tools include/bmk/machine rumprun.o commonlibs buildtest

include ../../global.mk
-include config.mk
ifdef BUILDRUMP_TOOLFLAGS
include ${BUILDRUMP_TOOLFLAGS}
endif

include ../Makefile.inc

CPPFLAGS=	-Iinclude -Irump/include -I../../include -nostdinc
CFLAGS+=	${BUILDRUMP_TOOL_CFLAGS}

# Check if we're building for a supported target.
supported= false
HASPCI= y
# assume we're doing "make clean"
MACHINE?= i386
ifeq (${MACHINE},i386)
supported:= true
endif
ifeq (${MACHINE},amd64)
supported:= true
endif
ifeq (${MACHINE},arm)
ifdef IWANTARM
supported:= true
HASPCI:= n
endif
endif
ifneq (${supported},true)
$(error only supported target is x86, you have ${MACHINE})
endif

ARCHDIR?= ${MACHINE}

LDFLAGS_BAKE+=	-L$(abspath rump/lib)

OBJS_BMK-y+=		intr.o clock.o clock_subr.o kernel.o multiboot.o undefs.o

OBJS_BMK+= ${OBJS_BMK-y}

include arch/${ARCHDIR}/Makefile.inc

OBJS_BMK+=	init.o

OBJS= ${OBJS_BMK} ${OBJS_APP}

.PHONY:	clean cleandir test

APP_TOOLS_PLATFORM= bmk
APP_TOOLS_OBJS_BAKE=	$(abspath rumprun.o)
APP_TOOLS_LDSCRIPT_BAKE:= $(abspath ${LDSCRIPT})
APP_TOOLS_DIR:= $(abspath ../../app-tools)

include ${APP_TOOLS_DIR}/Makefile.app-tools

include/bmk/machine:
	@ln -s ../arch/${ARCHDIR} include/bmk/machine

links: include/bmk/machine

rumprun.o: ${OBJS}
	${CC} -nostdlib ${CFLAGS} -Wl,-r ${OBJS_BMK} -o $@
	${OBJCOPY} -w -G bmk_* -G rumpuser_* \
	    -G rumprun_platform_rumpuser_init -G _start $@

clean: app-tools_clean commonclean
	rm -f ${OBJS} include/bmk/machine buildtest

cleandir: clean

cleanrump: clean
	rm -rf rump rumpobj rumptools

distcleanrump: cleanrump
	rm -f config.mk
	${MAKE} -C ../../tests clean
