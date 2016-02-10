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
MACHINE_GNU_PLATFORM:=\
    $(shell ${RUMPMAKE} -f bsd.own.mk -V '$${MACHINE_GNU_PLATFORM}')

_FILES=	toolchain.cmake recipe.s
FILES=	$(_FILES:%=$(TOOLTUPLE)-%)

SPECS_ARCH:= specs-compile_or_ferment specs-stub
SPECS_MACH:= specs-bake
SPECS:= ${SPECS_ARCH} ${SPECS_MACH}

_BIN_G=	ar as cpp ld nm objcopy objdump ranlib readelf size strings strip
BIN_G=	$(_BIN_G:%=$(TOOLTUPLE)-%)
BIN_G+=	$(TOOLTUPLE)-$(_CCVARIANT)
CCWRP=	${CC}
ifeq (${CONFIG_CXX},yes)
BIN_G+=	$(TOOLTUPLE)-$(_CXXVARIANT)
CCWRP+=	${CXX}
endif
BIN_G+=	rumprun-bake
BIN_G+= $(TOOLTUPLE)-genfs
STATICBIN= rumprun rumpstop

GENS.bin=	${BIN_G:%=${TOOLOBJ}/%}
GENS.files=	${FILES:%=${TOOLOBJ}/%}
GENS.archspecs=	${SPECS_ARCH:%=${TOOLOBJ}/%}
GENS.machspecs=	${SPECS_MACH:%=${TOOLOBJ}/%}
GENS=		${GENS.bin} ${GENS.files} ${GENS.archspecs} ${GENS.machspecs}

.PHONY: app-tools
app-tools: ${GENS}

include ../global.mk
LDFLAGS_BAKE:= ${LDFLAGS.${MACHINE_GNU_ARCH}.${PLATFORM}}

BRSOURCEDIR:=$(dir ${AR})
BRLIBEXECINST:= ${INSTALLDIR}/libexec/rumprun-${MACHINE_GNU_ARCH}-${PLATFORM}/
BRLIBEXECDEST:= ${RRDEST}/libexec/rumprun-${MACHINE_GNU_ARCH}-${PLATFORM}/

INSTALLDIR_ARCH:= ${INSTALLDIR}/rumprun-${MACHINE_GNU_ARCH}

ARCHDIR:= rumprun-${MACHINE_GNU_ARCH}
MACHDIR:= rumprun-${MACHINE_GNU_ARCH}-${PLATFORM}

LIBEXEC_CC:= $(subst ${BRSOURCEDIR},${BRLIBEXECDEST},${CC})
LIBEXEC_CXX:= $(subst ${BRSOURCEDIR},${BRLIBEXECDEST},${CXX})
LIBEXEC_OBJCOPY:= ${BRLIBEXECDEST}/${MACHINE_GNU_PLATFORM}-objcopy
LIBEXEC_READELF:= ${BRLIBEXECDEST}/${MACHINE_GNU_PLATFORM}-readelf

TOOLS_CC := ${RRDEST}/bin/$(TOOLTUPLE)-${_CCVARIANT}
TOOLS_CXX := ${RRDEST}/bin/$(TOOLTUPLE)-${_CXXVARIANT}

${TOOLOBJ}:
	mkdir -p "$@"

define templ
${TOOLOBJ}/${2}: ${1} Makefile ${TOOLOBJ}
	sed <$$< > $$@							\
		-e 's#!LIBEXEC_CC!#$(LIBEXEC_CC)#g;'			\
		-e 's#!LIBEXEC_CXX!#$(LIBEXEC_CXX)#g;'			\
		-e 's#!LIBEXEC_OBJCOPY!#$(LIBEXEC_OBJCOPY)#g;'		\
		-e 's#!LIBEXEC_READELF!#$(LIBEXEC_READELF)#g;'		\
		-e 's#!TOOLS_CC!#$(TOOLS_CC)#g;'			\
		-e 's#!TOOLS_CXX!#$(TOOLS_CXX)#g;'			\
		-e 's#!MACHINE_GNU_ARCH!#$(MACHINE_GNU_ARCH)#g;'	\
		-e 's#!TOOLTUPLE!#$(TOOLTUPLE)#g;'			\
		-e 's#!DESTDIR!#$(RRDEST)#g;'				\
		-e 's#!PLATFORM!#$(PLATFORM)#g;'			\
		-e 's#!CPPFLAGS!#$(BUILDRUMP_TOOL_CPPFLAGS)#g;'		\
		-e 's#!CFLAGS!#$(BUILDRUMP_TOOL_CFLAGS)#g;'		\
		-e 's#!CXXFLAGS!#$(BUILDRUMP_TOOL_CXXFLAGS)#g;'		\
		-e 's#!LDFLAGS_BAKE!#$(LDFLAGS_BAKE)#g;'
	chmod 755 $$@
endef
$(foreach file,${_FILES},$(eval $(call templ,${file}.in,${TOOLTUPLE}-${file})))
$(foreach spec,${SPECS},$(eval $(call templ,${spec}.in,${spec})))
$(eval $(call templ,cc.in,$(TOOLTUPLE)-${_CCVARIANT}))
$(eval $(call templ,cc.in,$(TOOLTUPLE)-${_CXXVARIANT}))
$(eval $(call templ,specs-bake.in,specs-bake-$(TOOLTUPLE)-$(PLATFORM)))
$(eval $(call templ,rumprun-bake.in,rumprun-bake))
$(eval $(call templ,genfs.in,$(TOOLTUPLE)-genfs))

define binwrapper
uptool_$1=$(shell echo $1 | tr '[:lower:]' '[:upper:]')
toolval_$1=$${$${uptool_$1}}
${TOOLOBJ}/$(TOOLTUPLE)-$1: ${TOOLOBJ}
	printf '#!/bin/sh\n\nexec %s "$$$${@}"\n' \
	    $$(subst ${BRSOURCEDIR},${BRLIBEXECDEST},$${toolval_${1}}) > $$@
	chmod 755 $$@

BRTOOLS+= $${toolval_${1}}
endef
$(foreach t,${_BIN_G},$(eval $(call binwrapper,${t})))

${BRLIBEXECINST}:
	mkdir -p $@

${INSTALLDIR}/etc:
	mkdir -p $@

${INSTALLDIR_ARCH}/share:
	mkdir -p $@

install-buildrumpwrappers: ${BRLIBEXECINST} ${BRTOOLS}
	install -m 0755 ${BRTOOLS} ${CCWRP} ${BRLIBEXECINST}
	install -m 0755 ${BRSOURCEDIR}/brprintmetainfo ${BRLIBEXECINST}

install-bins: ${GENS.bin} ${STATICBIN}
	install -m 0755 ${GENS.bin} ${INSTALLDIR}/bin
	install -m 0755 ${STATICBIN} ${INSTALLDIR}/bin
	ln -f ${INSTALLDIR}/bin/rumprun-bake ${INSTALLDIR}/bin/rumpbake

install-files: ${GENS.files} ${GENS.archspecs} ${GENS.machspecs} \
    ${INSTALLDIR}/etc ${INSTALLDIR_ARCH}/share
	install -m 0644 ${GENS.files} ${INSTALLDIR_ARCH}/share
	install -m 0644 ${GENS.archspecs} ${INSTALLDIR_ARCH}/lib
	install -m 0644 ${GENS.machspecs} ${INSTALLDIR_ARCH}/lib/rumprun-${PLATFORM}
	install -m 0644 rumprun-bake.conf ${INSTALLDIR}/etc

install: install-buildrumpwrappers install-bins install-files

clean:
	rm -f ${GENS}
