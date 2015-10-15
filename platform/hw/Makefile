PLATFORM=hw
PLATFORM_DEFAULT_TESTER=qemu

include ../../global.mk
-include config.mk
ifdef BUILDRUMP_TOOLFLAGS
include ${BUILDRUMP_TOOLFLAGS}
endif

ifneq (${KERNONLY},true)
TARGETS:= app-tools userlibs
else
TARGETS:= compiler_rt
endif

MAINOBJ=	rumprun-hw-${MACHINE_ARCH}.o

all:  include/hw/machine ${MAINOBJ} ${TARGETS}

CPPFLAGS=	-Iinclude -I../../include -nostdinc
CFLAGS+=	${BUILDRUMP_TOOL_CFLAGS}

ifeq (${BUILDRR},true)
CPPFLAGS+=	-I${OBJDIR}/dest.stage/include
else
CPPFLAGS+=	-I${DESTDIR}/include
endif

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

LDSCRIPT:=	$(abspath arch/${ARCHDIR}/kern.ldscript)
OBJS_BMK+=	intr.o clock_subr.o kernel.o multiboot.o undefs.o

include ../Makefile.inc

include arch/${ARCHDIR}/Makefile.inc

.PHONY:	clean cleandir test

APP_TOOLS_PLATFORM= hw
APP_TOOLS_DIR:= $(abspath ../../app-tools)

include ${APP_TOOLS_DIR}/Makefile.app-tools

include/hw/machine:
	@ln -s ../arch/${MACHINE_ARCH} include/hw/machine

../../include/bmk-pcpu:
	@ln -sf ../platform/hw/include/hw/machine $@

links: include/hw/machine ../../include/bmk-pcpu

${MAINOBJ}: ${OBJS_BMK} platformlibs
	${CC} -nostdlib ${CFLAGS} -Wl,-r ${OBJS_BMK} -o $@ \
	    -L${COREDIR}/hw -L${RUMPUSERDIR}/hw	-Wl,--whole-archive \
	    -lbmk_rumpuser -lbmk_core -Wl,--no-whole-archive
	${OBJCOPY} -w -G bmk_* -G rumpuser_* -G jsmn_* \
	    -G rumprun_platform_rumpuser_init -G _start $@

clean: app-tools_clean commonclean
	rm -f ${OBJS_BMK} include/hw/machine buildtest ${MAINOBJ}

cleandir: clean

cleanrump: clean
	rm -rf rump rumpobj rumptools

distcleanrump: cleanrump
	rm -f config.mk
	${MAKE} -C ../../tests clean
