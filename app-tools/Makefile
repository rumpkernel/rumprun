ifeq ($(RUMPRUN_MKCONF),)
$(error RUMPRUN_MKCONF missing)
endif

include ${RUMPRUN_MKCONF}
ifeq (${BUILDRUMP_TOOLFLAGS},)
$(error invalid config file ${RUMPRUN_MKCONF})
endif
include ${BUILDRUMP_TOOLFLAGS}

_CCVARIANT:=$(shell ${RUMPMAKE} -f bsd.own.mk -V '$${HAVE_LLVM:Dclang:Ugcc}')
_CXXVARIANT:=$(shell ${RUMPMAKE} -f bsd.own.mk -V '$${HAVE_LLVM:Dclang++:Ug++}')

_FILES=	toolchain.cmake recipe.s
FILES=	$(_FILES:%=$(TOOLTUPLE)-%)

_SPECS=	specs-compile_or_ferment specs-stub
SPECS=	${_SPECS} specs-bake-${TOOLTUPLE}-${PLATFORM}

_BINS=	ar as cpp ld nm objcopy objdump ranlib readelf size strings strip
BINS=	$(_BINS:%=$(TOOLTUPLE)-%)
BINS+=	$(TOOLTUPLE)-$(_CCVARIANT) $(TOOLTUPLE)-$(_CXXVARIANT)
BINS+=	rumprun-bake rumprun rumpstop

.PHONY: app-tools
app-tools: ${FILES} ${SPECS} ${BINS}

include ../global.mk
LDFLAGS_BAKE:= ${LDFLAGS.${PLATFORM}.${MACHINE_ARCH}}

BRSOURCEDIR:=$(dir ${AR})
BRLIBEXECDIR:= ${INSTALLDIR}/libexec/rumprun-${PLATFORM}-${MACHINE_ARCH}/

LIBEXEC_CC:= $(subst ${BRSOURCEDIR},${BRLIBEXECDIR},${CC})
LIBEXEC_CXX:= $(subst ${BRSOURCEDIR},${BRLIBEXECDIR},${CXX})

TOOLS_CC := ${RRDEST}/bin/$(TOOLTUPLE)-${_CCVARIANT}
TOOLS_CXX := ${RRDEST}/bin/$(TOOLTUPLE)-${_CXXVARIANT}

define templ
${2}: ${1} Makefile
	sed <$$< >$$@.tmp						\
		-e 's#!CC!#$(LIBEXEC_CC)#g;'				\
		-e 's#!CXX!#$(LIBEXEC_CXX)#g;'				\
		-e 's#!APPTOOLS_CC!#$(TOOLS_CC)#g;'			\
		-e 's#!APPTOOLS_CXX!#$(TOOLS_CXX)#g;'			\
		-e 's#!MACHINE_ARCH!#$(MACHINE_ARCH)#g;'		\
		-e 's#!GNUPLATFORM!#$(TOOLTUPLE)#g;'			\
		-e 's#!DESTDIR!#$(RRDEST)#g;'				\
		-e 's#!APPTOOLS_PLATFORM!#$(PLATFORM)#g;'		\
		-e 's#!CPPFLAGS!#$(BUILDRUMP_TOOL_CPPFLAGS)#g;'		\
		-e 's#!CFLAGS!#$(BUILDRUMP_TOOL_CFLAGS)#g;'		\
		-e 's#!CXXFLAGS!#$(BUILDRUMP_TOOL_CXXFLAGS)#g;'		\
		-e 's#!LDFLAGS_BAKE!#$(LDFLAGS_BAKE)#g;'
	mv -f $$@.tmp $$@
endef
$(foreach file,${_FILES},$(eval \
    $(call templ,${file}.in,$(TOOLTUPLE)-${file})))
$(foreach spec,${_SPECS},$(eval \
    $(call templ,${spec}.in,${spec})))
$(eval $(call templ,cc.in,$(TOOLTUPLE)-${_CCVARIANT}))
$(eval $(call templ,cc.in,$(TOOLTUPLE)-${_CXXVARIANT}))
$(eval $(call templ,specs-bake.in,specs-bake-$(TOOLTUPLE)-$(PLATFORM)))
$(eval $(call templ,rumprun-bake.in,rumprun-bake))

define binwrapper
uptool_$1=$(shell echo $1 | tr '[:lower:]' '[:upper:]')
toolval_$1=$${$${uptool_$1}}
$(TOOLTUPLE)-$1:
	printf '#!/bin/sh\n\nexec %s "$$$${@}"\n' \
	    $$(subst ${BRSOURCEDIR},${BRLIBEXECDIR},$${toolval_${1}}) >$$@

BRTOOLS+= $${toolval_${1}}
endef
$(foreach t,${_BINS},$(eval $(call binwrapper,${t})))

${BRLIBEXECDIR}:
	mkdir -p $@

${INSTALLDIR}/etc:
	mkdir -p $@

install-buildrumpwrappers: ${BRLIBEXECDIR} ${BRTOOLS}
	install -m 0755 ${BRTOOLS} ${CC} ${CXX} ${BRLIBEXECDIR}

install-bins: ${BINS}
	install -m 0755 ${BINS} ${INSTALLDIR}/bin
	ln -f ${INSTALLDIR}/bin/rumprun-bake ${INSTALLDIR}/bin/rumpbake

install-files: ${SPECS} ${FILES} ${INSTALLDIR}/etc
	install -m 0644 ${FILES} ${SPECS} ${INSTALLDIR}/share
	install -m 0644 rumprun-bake.conf ${INSTALLDIR}/etc

install: install-buildrumpwrappers install-bins install-files

clean:
	rm -f ${FILES} ${SPECS} ${BINS}
