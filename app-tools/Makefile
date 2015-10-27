ifeq ($(RUMPRUN_MKCONF),)
$(error RUMPRUN_MKCONF missing)
endif

include ${RUMPRUN_MKCONF}
ifeq (${BUILDRUMP_TOOLFLAGS},)
$(error invalid config file ${RUMPRUN_MKCONF})
endif
include ${BUILDRUMP_TOOLFLAGS}

TOOLOBJ:= ${RROBJ}/app-tools

_CCVARIANT:=$(shell ${RUMPMAKE} -f bsd.own.mk -V '$${HAVE_LLVM:Dclang:Ugcc}')
_CXXVARIANT:=$(shell ${RUMPMAKE} -f bsd.own.mk -V '$${HAVE_LLVM:Dclang++:Ug++}')

_FILES=	toolchain.cmake recipe.s
FILES=	$(_FILES:%=$(TOOLTUPLE)-%)

_SPECS=	specs-compile_or_ferment specs-stub
SPECS=	${_SPECS} specs-bake-${TOOLTUPLE}-${PLATFORM}

_BIN_G=	ar as cpp ld nm objcopy objdump ranlib readelf size strings strip
BIN_G=	$(_BIN_G:%=$(TOOLTUPLE)-%)
BIN_G+=	$(TOOLTUPLE)-$(_CCVARIANT) $(TOOLTUPLE)-$(_CXXVARIANT)
BIN_G+=	rumprun-bake
STATICBIN= rumprun rumpstop

GENS.bin=	${BIN_G:%=${TOOLOBJ}/%}
GENS.files=	${FILES:%=${TOOLOBJ}/%}
GENS.specs=	${SPECS:%=${TOOLOBJ}/%}
GENS=		${GENS.bin} ${GENS.files} ${GENS.specs}

.PHONY: app-tools
app-tools: ${GENS}

include ../global.mk
LDFLAGS_BAKE:= ${LDFLAGS.${PLATFORM}.${MACHINE_ARCH}}

BRSOURCEDIR:=$(dir ${AR})
BRLIBEXECINST:= ${INSTALLDIR}/libexec/rumprun-${PLATFORM}-${MACHINE_ARCH}/
BRLIBEXECDEST:= ${RRDEST}/libexec/rumprun-${PLATFORM}-${MACHINE_ARCH}/

LIBEXEC_CC:= $(subst ${BRSOURCEDIR},${BRLIBEXECDEST},${CC})
LIBEXEC_CXX:= $(subst ${BRSOURCEDIR},${BRLIBEXECDEST},${CXX})

TOOLS_CC := ${RRDEST}/bin/$(TOOLTUPLE)-${_CCVARIANT}
TOOLS_CXX := ${RRDEST}/bin/$(TOOLTUPLE)-${_CXXVARIANT}

${TOOLOBJ}:
	mkdir -p "$@"

define templ
${TOOLOBJ}/${2}: ${1} Makefile ${TOOLOBJ}
	sed <$$< > $$@							\
		-e 's#!LIBEXEC_CC!#$(LIBEXEC_CC)#g;'			\
		-e 's#!LIBEXEC_CXX!#$(LIBEXEC_CXX)#g;'			\
		-e 's#!TOOLS_CC!#$(TOOLS_CC)#g;'			\
		-e 's#!TOOLS_CXX!#$(TOOLS_CXX)#g;'			\
		-e 's#!MACHINE_ARCH!#$(MACHINE_ARCH)#g;'		\
		-e 's#!TOOLTUPLE!#$(TOOLTUPLE)#g;'			\
		-e 's#!DESTDIR!#$(RRDEST)#g;'				\
		-e 's#!PLATFORM!#$(PLATFORM)#g;'			\
		-e 's#!CPPFLAGS!#$(BUILDRUMP_TOOL_CPPFLAGS)#g;'		\
		-e 's#!CFLAGS!#$(BUILDRUMP_TOOL_CFLAGS)#g;'		\
		-e 's#!CXXFLAGS!#$(BUILDRUMP_TOOL_CXXFLAGS)#g;'		\
		-e 's#!LDFLAGS_BAKE!#$(LDFLAGS_BAKE)#g;'
endef
$(foreach file,${_FILES},$(eval $(call templ,${file}.in,${TOOLTUPLE}-${file})))
$(foreach spec,${_SPECS},$(eval $(call templ,${spec}.in,${spec})))
$(eval $(call templ,cc.in,$(TOOLTUPLE)-${_CCVARIANT}))
$(eval $(call templ,cc.in,$(TOOLTUPLE)-${_CXXVARIANT}))
$(eval $(call templ,specs-bake.in,specs-bake-$(TOOLTUPLE)-$(PLATFORM)))
$(eval $(call templ,rumprun-bake.in,rumprun-bake))

define binwrapper
uptool_$1=$(shell echo $1 | tr '[:lower:]' '[:upper:]')
toolval_$1=$${$${uptool_$1}}
${TOOLOBJ}/$(TOOLTUPLE)-$1: ${TOOLOBJ}
	printf '#!/bin/sh\n\nexec %s "$$$${@}"\n' \
	    $$(subst ${BRSOURCEDIR},${BRLIBEXECDEST},$${toolval_${1}}) > $$@

BRTOOLS+= $${toolval_${1}}
endef
$(foreach t,${_BIN_G},$(eval $(call binwrapper,${t})))

${BRLIBEXECINST}:
	mkdir -p $@

${INSTALLDIR}/etc:
	mkdir -p $@

install-buildrumpwrappers: ${BRLIBEXECINST} ${BRTOOLS}
	install -m 0755 ${BRTOOLS} ${CC} ${CXX} ${BRLIBEXECINST}

install-bins: ${GENS.bin} ${STATICBIN}
	install -m 0755 ${GENS.bin} ${INSTALLDIR}/bin
	install -m 0755 ${STATICBIN} ${INSTALLDIR}/bin
	ln -f ${INSTALLDIR}/bin/rumprun-bake ${INSTALLDIR}/bin/rumpbake

install-files: ${GENS.files} ${GENS.specs} ${INSTALLDIR}/etc
	install -m 0644 ${GENS.files} ${GENS.specs} ${INSTALLDIR}/share
	install -m 0644 rumprun-bake.conf ${INSTALLDIR}/etc

install: install-buildrumpwrappers install-bins install-files

clean:
	rm -f ${GENS}
