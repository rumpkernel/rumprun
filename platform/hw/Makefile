default: all

PLATFORM_DEFAULT_TESTER=qemu

include ../../global.mk
-include config.mk
ifdef BUILDRUMP_TOOLFLAGS
include ${BUILDRUMP_TOOLFLAGS}
endif

CPPFLAGS=	-Iinclude -I../../include -nostdinc
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

LDSCRIPT:=	$(abspath arch/${ARCHDIR}/kern.ldscript)
OBJS_BMK+=	intr.o clock_subr.o kernel.o multiboot.o undefs.o

include ../Makefile.inc
include arch/${ARCHDIR}/Makefile.inc

.PHONY:	clean cleandir test all

all:  include/hw/machine ${MAINOBJ} ${TARGETS}

include/hw/machine:
	@ln -sf ../arch/${MACHINE_ARCH} include/hw/machine

../../include/bmk-pcpu:
	@ln -sf ../platform/hw/include/hw/machine $@

links: include/hw/machine ../../include/bmk-pcpu

${MAINOBJ}: ${OBJS_BMK} platformlibs
	${CC} -nostdlib ${CFLAGS} -Wl,-r ${OBJS_BMK} -o $@ \
	    -L${COREDIR}/hw -L${RUMPUSERDIR}/hw	-Wl,--whole-archive \
	    -lbmk_rumpuser -lbmk_core -Wl,--no-whole-archive
	${OBJCOPY} -w -G bmk_* -G rumpuser_* -G jsmn_* \
	    -G rumprun_platform_rumpuser_init -G _start $@

clean: commonclean
	rm -f ${OBJS_BMK} include/hw/machine buildtest ${MAINOBJ}

cleandir: clean

cleanrump: clean
	rm -rf rump rumpobj rumptools

distcleanrump: cleanrump
	rm -f config.mk
	${MAKE} -C ../../tests clean
