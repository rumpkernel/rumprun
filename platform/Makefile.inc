ifeq (${PLATFORM},)
$(error need to specify $$PLATFORM!)
endif

TARGETS=	rumpkernlibs
# compiler_rt is strictly speaking necessary only in KERNONLY=true,
# but building it always makes testing kernonly easier
TARGETS+=	compiler_rt
INSTALLTGTS=	librumpkern_bmktc_install

ifneq (${KERNONLY},true)
TARGETS+=	userlibs
INSTALLTGTS+=	librumprun_base_install librumprun_tester_install2
INSTALLTGTS+=	libunwind_install2
endif

ifeq (${BUILDRR},true)
CPPFLAGS+=      -I${RROBJ}/dest.stage/include
else
CPPFLAGS+=      -I${RRDEST}/include/rumprun
endif

MAINOBJ:=	${RROBJ}/rumprun-${PLATFORM}-${MACHINE_ARCH}.o

RROBJLIB:=	${RROBJ}/lib
LIBDIR:=	$(abspath ../../lib)

COMMONDIR:=	$(abspath ../)

define BUILDLIB_target
.PHONY: $${RROBJLIB}/${1}/${1}.a
$${RROBJLIB}/${1}/${1}.a:
	( cd $${LIBDIR}/${1} \
	    && ${RUMPMAKE} MAKEOBJDIR=${RROBJLIB}/${1} ${2} obj \
	    && ${RUMPMAKE} MAKEOBJDIR=${RROBJLIB}/${1} ${2} dependall )

.PHONY: ${1}_install
${1}_install:
	( cd $${RROBJLIB}/${1} && \
	    install -m 0444 ${1}.a \
	      $${INSTALLDIR}/lib/rumprun-${PLATFORM}-${MACHINE_ARCH} )

.PHONY: ${1}_install2
${1}_install2:
	( cd $${RROBJLIB}/${1} && \
	    install -m 0444 ${1}.a \
	      $${INSTALLDIR}/lib/rumprun-${MACHINE_ARCH} )

.PHONY: ${1}_clean
${1}_clean:
	( cd $${LIBDIR}/${1} && \
	    ${RUMPMAKE} MAKEOBJDIR=${RROBJLIB}/${1} ${2} cleandir )
endef

$(eval $(call BUILDLIB_target,libbmk_core))
$(eval $(call BUILDLIB_target,libbmk_rumpuser))
$(eval $(call BUILDLIB_target,librumpkern_bmktc))
$(eval $(call BUILDLIB_target,librumprun_base))
$(eval $(call BUILDLIB_target,librumprun_tester))
$(eval $(call BUILDLIB_target,libunwind))
$(eval $(call BUILDLIB_target,libcompiler_rt,RUMPSRC=${RUMPSRC}))

PSEUDOSTUBS=${RROBJ}/rumprun-${MACHINE_ARCH}-pseudolinkstubs

${PSEUDOSTUBS}.c: ${RROBJLIB}/librumprun_base/librumprun_base.a
	sh ../makepseudolinkstubs.sh ${NM} ${RUMPSRC} $< $@

bmk.ldscript: ${LDSCRIPT}
	ln -sf $< $@

commonlibs: platformlibs userlibs
userlibs: ${PSEUDOSTUBS}.o ${RROBJLIB}/librumprun_base/librumprun_base.a ${RROBJLIB}/librumprun_tester/librumprun_tester.a ${RROBJLIB}/libunwind/libunwind.a
platformlibs: ${RROBJLIB}/libbmk_core/libbmk_core.a ${RROBJLIB}/libbmk_rumpuser/libbmk_rumpuser.a bmk.ldscript
rumpkernlibs: ${RROBJLIB}/librumpkern_bmktc/librumpkern_bmktc.a
compiler_rt: ${RROBJLIB}/libcompiler_rt/libcompiler_rt.a

.PHONY: buildtest
buildtest: ../../tests/hello/hello.c
	${TOOLTUPLE}-gcc -g -o $@ $< -lrumprun_tester
	@echo Testing baking ...
	@export RUMPRUN_WARNING_STFU=please ; for board in \
	    $(shell RUMPRUN_WARNING_STFU=please \
		rumpbake list \
		| awk '/^${PLATFORM}/{print $$1}'); do \
			echo $${board} ; \
			rumpbake $${board} $@.$${board} $@ || exit 1;\
	done
	@echo done

.PHONY: commonclean
commonclean: libbmk_core_clean libbmk_rumpuser_clean libcompiler_rt_clean
	rm -f ${PSEUDOSTUBS}.c ${PSEUDOSTUBS}.o
	rm -f bmk.ldscript

.PHONY: tests
tests: ${MAINOBJ} commonlibs
	../../tests/buildtests.sh ${PLATFORM}
	../../tests/runtests.sh ${PLATFORM_DEFAULT_TESTER}

.PHONY: installkern
installkern:
	install -m 0444 ${MAINOBJ} ${INSTALLDIR}/lib
	install -m 0444 ${LDSCRIPT} ${INSTALLDIR}/lib/rumprun-${PLATFORM}-${MACHINE_ARCH}.ldscript

.PHONY: installuser
installuser:
	install -m 0444 ${PSEUDOSTUBS}.o ${INSTALLDIR}/lib

INSTALLTGTS+=	installkern
ifneq (${KERNONLY},true)
INSTALLTGTS+=	installuser
endif

.PHONY: install
install: ${INSTALLTGTS}
