ifeq (${PLATFORM},)
$(error need to specify $$PLATFORM!)
endif

APP_TOOLS_GNUPLATFORM:=$(shell ${RUMPMAKE} -f bsd.own.mk \
    -V '$${MACHINE_GNU_PLATFORM}')

COREDIR:=       $(shell pwd)/../../lib/libbmk_core
RUMPUSERDIR:=   $(shell pwd)/../../lib/libbmk_rumpuser
BASEDIR:=       $(shell pwd)/../../lib/librumprun_base

COMMONDIR:=	$(abspath ../)

LDFLAGS_BAKE+=	-L${BASEDIR}/${PLATFORM} -L${COREDIR}/${PLATFORM}	\
		-L${RUMPUSERDIR}/${PLATFORM}

.PHONY: ${BASEDIR}/${PLATFORM}/librumprun_base.a
${BASEDIR}/${PLATFORM}/librumprun_base.a:
	( cd ${BASEDIR} \
	    && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} obj \
	    && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} dependall )

${COMMONDIR}/pseudolinkstubs.c: ${BASEDIR}/${PLATFORM}/librumprun_base.a
	sh ../makepseudolinkstubs.sh ${NM} ${RUMPSRC} $< $@

.PHONY: ${COREDIR}/${PLATFORM}/libbmk_core.a
${COREDIR}/${PLATFORM}/libbmk_core.a:
	( cd ${COREDIR} \
	    && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} obj \
	    && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} dependall )

.PHONY: ${RUMPUSERDIR}/${PLATFORM}/libbmk_rumpuser.a
${RUMPUSERDIR}/${PLATFORM}/libbmk_rumpuser.a:
	( cd ${RUMPUSERDIR} \
	    && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} obj \
	    && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} dependall )

commonlibs: ${BASEDIR}/${PLATFORM}/librumprun_base.a ${COREDIR}/${PLATFORM}/libbmk_core.a ${RUMPUSERDIR}/${PLATFORM}/libbmk_rumpuser.a ${COMMONDIR}/pseudolinkstubs.o

.PHONY: buildtest
buildtest: ../../tests/hello/hello.c ${OBJ_DIR}/rumprun.o commonlibs app-tools
	${APP_TOOLS_CC} -g -o $@ $< -lrumprun_tester
	@echo Testing baking ...
	@export RUMPRUN_WARNING_STFU=please ; for board in \
	    $(shell RUMPRUN_WARNING_STFU=please ${APP_TOOLS_DIR}/rumpbake list\
		| grep ^${PLATFORM}); do \
			echo $${board} ; \
			${APP_TOOLS_DIR}/rumpbake $${board} $@.$${board} $@ \
			    || exit 1;\
	done
	@echo done

.PHONY: commonclean
commonclean:
	( cd ${BASEDIR} && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} cleandir )
	( cd ${COREDIR} && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} cleandir )
	( cd ${RUMPUSERDIR} && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} cleandir )
	rm -f ${COMMONDIR}/pseudolinkstubs.c ${COMMONDIR}/pseudolinkstubs.o

.PHONY: tests
tests: ${OBJ_DIR}/rumprun.o commonlibs app-tools
	../../tests/buildtests.sh ${PLATFORM}
	../../tests/runtests.sh ${PLATFORM_DEFAULT_TESTER}
