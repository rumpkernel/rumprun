PLATFORM=hw
PLATFORM_DEFAULT_TESTER=qemu
OBJ_DIR=.

include ../../global.mk
-include config.mk
ifdef BUILDRUMP_TOOLFLAGS
include ${BUILDRUMP_TOOLFLAGS}
endif

ifneq (${KERNONLY},true)
TARGETS:= app-tools userlibs buildtest
else
TARGETS:= compiler_rt
endif

all:  include/hw/machine rumprun.o platformlibs ${TARGETS}

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
ifeq (${MACHINE},evbarm)
supported:= true
HASPCI:= n
ARCHDIR= arm/integrator
endif
ifneq (${supported},true)
$(error only supported target is x86, you have ${MACHINE})
endif

ARCHDIR?= ${MACHINE}

LDFLAGS_BAKE+=	-L$(abspath rump/lib)

OBJS_BMK-y+=		intr.o clock_subr.o kernel.o multiboot.o undefs.o

OBJS_BMK+= ${OBJS_BMK-y}

include arch/${ARCHDIR}/Makefile.inc

ifneq (${KERNONLY},true)
OBJS_BMK+=	init.o
endif

OBJS= ${OBJS_BMK} ${OBJS_APP}

.PHONY:	clean cleandir test

APP_TOOLS_PLATFORM= hw
APP_TOOLS_OBJS_BAKE=	$(abspath rumprun.o)
APP_TOOLS_LDSCRIPT_BAKE:= $(abspath ${LDSCRIPT})
APP_TOOLS_DIR:= $(abspath ../../app-tools)

include ${APP_TOOLS_DIR}/Makefile.app-tools

include/hw/machine:
	@ln -s ../arch/${MACHINE_ARCH} include/hw/machine

links: include/hw/machine

rumprun.o: ${OBJS}
	${CC} -nostdlib ${CFLAGS} -Wl,-r ${OBJS_BMK} -o $@
	${OBJCOPY} -w -G bmk_* -G rumpuser_* \
	    -G rumprun_platform_rumpuser_init -G _start $@

clean: app-tools_clean commonclean
	rm -f ${OBJS} include/hw/machine buildtest

cleandir: clean

cleanrump: clean
	rm -rf rump rumpobj rumptools

distcleanrump: cleanrump
	rm -f config.mk
	${MAKE} -C ../../tests clean
