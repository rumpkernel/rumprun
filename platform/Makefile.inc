ifeq (${PLATFORM},)
$(error need to specify $$PLATFORM!)
endif

CPPFLAGS+=	-Iinclude -I../../include -I${RROBJ}/include -nostdinc
CFLAGS+=	${BUILDRUMP_TOOL_CFLAGS}

TARGETS=	rumpkernlibs
# compiler_rt is strictly speaking necessary only in KERNONLY=true,
# but building it always makes testing kernonly easier
TARGETS+=	compiler_rt
INSTALLTGTS=	librumpkern_bmktc_install
INSTALLTGTS+=	librumpkern_mman_install

ifneq (${KERNONLY},true)
TARGETS+=	userlibs
INSTALLTGTS+=	librumprun_base_install librumprun_tester_install2
INSTALLTGTS+=	librumprunfs_base_install
ifeq (${CONFIG_CXX},yes)
INSTALLTGTS+=	libunwind_install2
endif
endif

ifeq (${BUILDRR},true)
CPPFLAGS+=      -I${RROBJ}/dest.stage/include
else
CPPFLAGS+=      -I${RRDEST}/rumprun-${MACHINE_GNU_ARCH}/include
endif

MAINOBJ:=	${RROBJ}/rumprun.o

RROBJLIB:=	${RROBJ}/lib
PLIBDIR:=	$(abspath ../../lib)

COMMONDIR:=	$(abspath ../)

INSTALLDIR_LIBARCH:=	${INSTALLDIR}/rumprun-${MACHINE_GNU_ARCH}/lib
INSTALLDIR_LIBPLAT:=	${INSTALLDIR}/rumprun-${MACHINE_GNU_ARCH}/lib/rumprun-${PLATFORM}

define BUILDLIB_target
.PHONY: $${RROBJLIB}/${1}/${1}.a
$${RROBJLIB}/${1}/${1}.a:
	( cd ${2}/${1} \
	    && ${RUMPMAKE} MAKEOBJDIR=${RROBJLIB}/${1} ${3} obj \
	    && ${RUMPMAKE} MAKEOBJDIR=${RROBJLIB}/${1} ${3} includes \
	    && ${RUMPMAKE} BMKHEADERS=$${RROBJ}/include \
		MAKEOBJDIR=${RROBJLIB}/${1} ${3} dependall )

.PHONY: ${1}_install
${1}_install:
	( cd $${RROBJLIB}/${1} && \
	    install -m 0444 ${1}.a $${INSTALLDIR_LIBPLAT} )

.PHONY: ${1}_install2
${1}_install2:
	( cd $${RROBJLIB}/${1} && \
	    install -m 0444 ${1}.a $${INSTALLDIR_LIBARCH} )

.PHONY: ${1}_clean
${1}_clean:
	( cd ${2}/${1} && \
	    ${RUMPMAKE} MAKEOBJDIR=${RROBJLIB}/${1} ${3} cleandir )
endef

$(eval $(call BUILDLIB_target,libbmk_core,${PLIBDIR}))
$(eval $(call BUILDLIB_target,libbmk_rumpuser,${PLIBDIR}))
$(eval $(call BUILDLIB_target,librumpkern_bmktc,${PLIBDIR}))
$(eval $(call BUILDLIB_target,librumpkern_mman,${PLIBDIR}))
$(eval $(call BUILDLIB_target,librumprun_base,${PLIBDIR}))
$(eval $(call BUILDLIB_target,librumprun_tester,${PLIBDIR}))
$(eval $(call BUILDLIB_target,librumprunfs_base,${PLIBDIR}))
$(eval $(call BUILDLIB_target,libcompiler_rt,${PLIBDIR},RUMPSRC=${RUMPSRC}))
ifeq (${CONFIG_CXX},yes)
$(eval $(call BUILDLIB_target,libunwind,${PLIBDIR}))
LIBUNWIND= ${RROBJLIB}/libunwind/libunwind.a
endif

PSEUDOSTUBS:= ${RROBJ}/pseudolinkstubs

${PSEUDOSTUBS}.c: ${RROBJLIB}/librumprun_base/librumprun_base.a
	sh ../makepseudolinkstubs.sh ${NM} ${RUMPSRC} $< $@

${RROBJ}/bmk.ldscript: ${LDSCRIPT}
	ln -sf $< $@

commonlibs: platformlibs userlibs
userlibs: ${PSEUDOSTUBS}.o ${RROBJLIB}/librumprun_base/librumprun_base.a ${RROBJLIB}/librumprun_tester/librumprun_tester.a ${LIBUNWIND} ${RROBJLIB}/librumprunfs_base/librumprunfs_base.a
platformlibs: ${RROBJLIB}/libbmk_core/libbmk_core.a ${RROBJLIB}/libbmk_rumpuser/libbmk_rumpuser.a ${RROBJ}/bmk.ldscript
rumpkernlibs: ${RROBJLIB}/librumpkern_bmktc/librumpkern_bmktc.a ${RROBJLIB}/librumpkern_mman/librumpkern_mman.a
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
	rm -f ${RROBJ}/bmk.ldscript

.PHONY: tests
tests: ${MAINOBJ} commonlibs
	../../tests/buildtests.sh ${PLATFORM}
	../../tests/runtests.sh ${PLATFORM_DEFAULT_TESTER}

.PHONY: installkern
installkern:
	install -m 0444 ${MAINOBJ} ${INSTALLDIR_LIBPLAT}
	install -m 0444 ${LDSCRIPT} ${INSTALLDIR}/rumprun-${MACHINE_GNU_ARCH}/lib/${PLATFORM}.ldscript

.PHONY: installuser
installuser:
	install -m 0444 ${PSEUDOSTUBS}.o ${INSTALLDIR_LIBARCH}

INSTALLTGTS+=	installkern
ifneq (${KERNONLY},true)
INSTALLTGTS+=	installuser
endif

.PHONY: install
install: ${INSTALLTGTS}
