ifeq (${PLATFORM},)
$(error need to specify $$PLATFORM!)
endif

COREDIR:=	$(shell pwd)/../../lib/libbmk_core
RUMPUSERDIR:=	$(shell pwd)/../../lib/libbmk_rumpuser
BASEDIR:=	$(shell pwd)/../../lib/librumprun_base
COMPILERRTDIR:=	$(shell pwd)/../../lib/libcompiler_rt

COMMONDIR:=	$(abspath ../)

LDFLAGS_BAKE+=	-L${BASEDIR}/${PLATFORM} -L${COREDIR}/${PLATFORM}	\
		-L${RUMPUSERDIR}/${PLATFORM}

define BUILDLIB_target
.PHONY: ${1}/${PLATFORM}/${2}
${1}/${PLATFORM}/${2}:
	( cd ${1} \
	    && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} ${3} obj \
	    && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} ${3} dependall )
endef

$(eval $(call BUILDLIB_target,${BASEDIR},librumprun_base.a))
$(eval $(call BUILDLIB_target,${COREDIR},libbmk_core.a))
$(eval $(call BUILDLIB_target,${RUMPUSERDIR},libbmk_rumpuser.a))
$(eval $(call BUILDLIB_target,${COMPILERRTDIR},libcompiler_rt.a,RUMPSRC=${RUMPSRC}))

${COMMONDIR}/pseudolinkstubs.c: ${BASEDIR}/${PLATFORM}/librumprun_base.a
	sh ../makepseudolinkstubs.sh ${NM} ${RUMPSRC} $< $@

commonlibs: platformlibs userlibs
userlibs: ${BASEDIR}/${PLATFORM}/librumprun_base.a ${COMMONDIR}/pseudolinkstubs.o
platformlibs: ${COREDIR}/${PLATFORM}/libbmk_core.a ${RUMPUSERDIR}/${PLATFORM}/libbmk_rumpuser.a
compiler_rt: ${COMPILERRTDIR}/${PLATFORM}/libcompiler_rt.a

.PHONY: buildtest
buildtest: ../../tests/hello/hello.c ${MAINOBJ} commonlibs app-tools
	${APP_TOOLS_CC} -g -o $@ $< -lrumprun_tester
	@echo Testing baking ...
	@export RUMPRUN_WARNING_STFU=please ; for board in \
	    $(shell RUMPRUN_WARNING_STFU=please \
		${APP_TOOLS_DIR}/rumpbake list \
		| awk '/^${PLATFORM}/{print $$1}'); do \
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
	( cd ${COMPILERRTDIR} && \
		${RUMPMAKE} RUMPSRC=${RUMPSRC} MAKEOBJDIR=${PLATFORM} cleandir )
	rm -f ${COMMONDIR}/pseudolinkstubs.c ${COMMONDIR}/pseudolinkstubs.o

.PHONY: tests
tests: ${MAINOBJ} commonlibs app-tools
	../../tests/buildtests.sh ${PLATFORM}
	../../tests/runtests.sh ${PLATFORM_DEFAULT_TESTER}
