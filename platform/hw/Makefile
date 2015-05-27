include ../../config.mk
include ../../global.mk
-include config.mk
ifdef BUILDRUMP_TOOLFLAGS
include ${BUILDRUMP_TOOLFLAGS}
endif

CPPFLAGS=	-Iinclude -Irump/include -I../../include -nostdinc
CFLAGS+=	${BUILDRUMP_TOOL_CFLAGS}

# Check if we're building for a supported target. For the time being,
# we build x86_64 in 32bit mode, because someone was lazy and did
# not write the 64bit bootstrap.
supported= false
HASPCI= y
# assume we're doing "make clean"
MACHINE?= i386
ifeq (${MACHINE},i386)
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

LDFLAGS:= -L$(abspath rump/lib)

ifeq (${CONFIG_SYSPROXY},yes)
LIBS_SYSPROXY=	-lrumpkern_sysproxy
endif

all: app-tools include/bmk/machine rumprun.o commonlibs buildtest

OBJS_BMK-y+=		intr.o clock.o kernel.o multiboot.o undefs.o
OBJS_BMK-${HASPCI}+=	rumppci.o

OBJS_BMK+= ${OBJS_BMK-y}

include arch/${MACHINE}/Makefile.inc

RUMPKERN_LIB=	-lrump

COREDIR:=	$(shell pwd)/../../lib/libbmk_core
RUMPUSERDIR:=	$(shell pwd)/../../lib/libbmk_rumpuser
BASEDIR:=	$(shell pwd)/../../lib/librumprun_base

OBJS_BMK+=	init.o
LIBS_USER=	-L${BASEDIR}/hw -L${COREDIR}/hw -L${RUMPUSERDIR}/hw --start-group -lrumprun_base -lpthread -lc --end-group -lbmk_rumpuser -lbmk_core
LIBS_USERSTUB=	-L${BASEDIR}/hw -L${COREDIR}/hw -L${RUMPUSERDIR}/hw --start-group -lrumprun_base -lpthread -lc --end-group -lbmk_core ${BASEDIR}/hw/librumprun_base_stubs.o
RUMP_LDLIBS=	--whole-archive ${LIBS_SYSPROXY} ${RUMPKERN_LIB} --no-whole-archive ${LIBS_USER}
RUMP_STUBLDLIBS=--whole-archive ${RUMPKERN_LIB} --no-whole-archive ${LIBS_USERSTUB}

OBJS= ${OBJS_BMK} ${OBJS_APP}

.PHONY:	clean cleandir test

APP_TOOLS_TARGETARCH=${MACHINE}
APP_TOOLS_PLATFORM= bmk
APP_TOOLS_HEADOBJ= $(abspath rumprun.o)
APP_TOOLS_OBJS=
APP_TOOLS_LDSCRIPT:= $(abspath ${LDSCRIPT})
APP_TOOLS_DIR:= $(abspath ../../app-tools)

include $(APP_TOOLS_DIR)/Makefile.app-tools

include/bmk/machine:
	ln -s ../arch/${MACHINE} include/bmk/machine

.PHONY: ${BASEDIR}/hw/librumprun_base.a
${BASEDIR}/hw/librumprun_base.a:
	( cd ${BASEDIR} \
	    && ${RUMPMAKE} MAKEOBJDIR=hw obj \
	    && ${RUMPMAKE} MAKEOBJDIR=hw dependall )

.PHONY: ${BASEDIR}/hw/librumprun_base_stubs.o
${BASEDIR}/hw/librumprun_base_stubs.o: ${BASEDIR}/librumprun_base_stubs.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: ${COREDIR}/hw/libbmk_core.a
${COREDIR}/hw/libbmk_core.a:
	( cd ${COREDIR} \
	    && ${RUMPMAKE} MAKEOBJDIR=hw obj \
	    && ${RUMPMAKE} MAKEOBJDIR=hw dependall )

.PHONY: ${RUMPUSERDIR}/hw/libbmk_rumpuser.a
${RUMPUSERDIR}/hw/libbmk_rumpuser.a:
	( cd ${RUMPUSERDIR} \
	    && ${RUMPMAKE} MAKEOBJDIR=hw obj \
	    && ${RUMPMAKE} MAKEOBJDIR=hw dependall )

rumprun.o: ${OBJS}
	${CC} -nostdlib ${CFLAGS} -Wl,-r ${OBJS_BMK} -o $@

commonlibs: ${BASEDIR}/hw/librumprun_base.a ${BASEDIR}/hw/librumprun_base_stubs.o ${COREDIR}/hw/libbmk_core.a ${RUMPUSERDIR}/hw/libbmk_rumpuser.a

buildtest: ../../tests/hello/hello.c rumprun.o commonlibs app-tools
	$(APP_TOOLS_CC) -g -o $@ $< -lrumprun_tester

tests: rumprun.o commonlibs app-tools
	RUMPRUN_PLATFORM=bmk ../../tests/buildtests.sh
	../../tests/runtests.sh qemu

clean: app-tools_clean
	rm -f ${OBJS} include/bmk/machine buildtest
	( cd ${BASEDIR} && ${RUMPMAKE} MAKEOBJDIR=hw cleandir )
	( cd ${COREDIR} && ${RUMPMAKE} MAKEOBJDIR=hw cleandir )
	( cd ${RUMPUSERDIR} && ${RUMPMAKE} MAKEOBJDIR=hw cleandir )

cleandir: clean

cleanrump: clean
	rm -rf rump rumpobj rumptools

distcleanrump: cleanrump
	rm -f config.mk
	$(MAKE) -C ../../tests clean
