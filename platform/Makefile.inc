ifeq (${PLATFORM},)
$(error need to specify $$PLATFORM!)
endif

COREDIR:=	$(shell pwd)/../../lib/libbmk_core
RUMPUSERDIR:=	$(shell pwd)/../../lib/libbmk_rumpuser
COMPILERRTDIR:=	$(shell pwd)/../../lib/libcompiler_rt

COMMONDIR:=	$(abspath ../)

define BUILDLIB_target
.PHONY: ${1}/${PLATFORM}/${2}
${1}/${PLATFORM}/${2}:
	( cd ${1} \
	    && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} ${3} obj \
	    && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} ${3} dependall )
endef

$(eval $(call BUILDLIB_target,${COREDIR},libbmk_core.a))
$(eval $(call BUILDLIB_target,${RUMPUSERDIR},libbmk_rumpuser.a))
$(eval $(call BUILDLIB_target,${COMPILERRTDIR},libcompiler_rt.a,RUMPSRC=${RUMPSRC}))

PSEUDOSTUBS=${COMMONDIR}/rumprun-pseudolinkstubs

${PSEUDOSTUBS}.c: ${OBJDIR}/dest.stage/lib/librumprun_base.a
	sh ../makepseudolinkstubs.sh ${NM} ${RUMPSRC} $< $@

bmk.ldscript: ${LDSCRIPT}
	ln -sf $< $@

commonlibs: platformlibs userlibs
userlibs: ${PSEUDOSTUBS}.o
platformlibs: ${COREDIR}/${PLATFORM}/libbmk_core.a ${RUMPUSERDIR}/${PLATFORM}/libbmk_rumpuser.a bmk.ldscript
compiler_rt: ${COMPILERRTDIR}/${PLATFORM}/libcompiler_rt.a

.PHONY: buildtest
buildtest: ../../tests/hello/hello.c ${MAINOBJ} commonlibs
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
	( cd ${COREDIR} && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} cleandir )
	( cd ${RUMPUSERDIR} && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} cleandir )
	( cd ${COMPILERRTDIR} && \
		${RUMPMAKE} RUMPSRC=${RUMPSRC} MAKEOBJDIR=${PLATFORM} cleandir )
	rm -f ${PSEUDOSTUBS}.c ${PSEUDOSTUBS}.o
	rm -f bmk.ldscript

.PHONY: tests
tests: ${MAINOBJ} commonlibs
	../../tests/buildtests.sh ${PLATFORM}
	../../tests/runtests.sh ${PLATFORM_DEFAULT_TESTER}

ifeq (${BUILDRR},true)
INSTALLDIR=	${OBJDIR}/dest.stage
else
INSTALLDIR=	${DESTDIR}
endif

.PHONY: installkern
installkern:
	install -m 0444 ${MAINOBJ} ${INSTALLDIR}/lib
	install -m 0444 ${LDSCRIPT} ${INSTALLDIR}/lib/rumprun-${PLATFORM}-${MACHINE_ARCH}.ldscript

.PHONY: installuser
installuser:
	install -m 0444 ${PSEUDOSTUBS}.o ${INSTALLDIR}/lib

INSTALLTGTS=	installkern
ifneq (${KERNONLY},true)
INSTALLTGTS+=	installuser
endif

.PHONY: install
install: ${INSTALLTGTS}
