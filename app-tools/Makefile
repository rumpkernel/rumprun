ifeq ($(RUMPRUN_MKCONF),)
$(error RUMPRUN_MKCONF missing)
endif

include ${RUMPRUN_MKCONF}
ifeq (${BUILDRUMP_TOOLFLAGS},)
$(error invalid config file ${RUMPRUN_MKCONF})
endif
include ${BUILDRUMP_TOOLFLAGS}

APP_TOOLS_DIR:=		$(abspath .)

APP_TOOL_FILES=		configure make gmake toolchain.cmake recipe.s

APP_TOOL_SPECS=		specs-compile_or_ferment specs-stub

APP_TOOL_PASSTHROUGH=	ar as cpp ld nm objcopy objdump ranlib readelf
APP_TOOL_PASSTHROUGH+=	size strings strip

_CCVARIANT:=$(shell ${RUMPMAKE} -f bsd.own.mk -V '$${HAVE_LLVM:Dclang:Ugcc}')
_CXXVARIANT:=$(shell ${RUMPMAKE} -f bsd.own.mk -V '$${HAVE_LLVM:Dclang++:Ug++}')

include ../global.mk
LDFLAGS_BAKE:= ${LDFLAGS.${PLATFORM}.${MACHINE_ARCH}}

_APP_TOOL_TARGETS:=							\
    $(APP_TOOL_FILES:%=$(APP_TOOLS_DIR)/$(TOOLTUPLE)-%)			\
    $(APP_TOOLS_DIR)/$(TOOLTUPLE)-$(_CCVARIANT)				\
    $(APP_TOOLS_DIR)/$(TOOLTUPLE)-$(_CXXVARIANT)			\
    $(APP_TOOL_SPECS:%=$(APP_TOOLS_DIR)/%)				\
    $(APP_TOOL_PASSTHROUGH:%=$(APP_TOOLS_DIR)/$(TOOLTUPLE)-%)		\
    $(APP_TOOLS_DIR)/specs-bake-$(TOOLTUPLE)-$(PLATFORM)

.PHONY: app-tools
app-tools: $(_APP_TOOL_TARGETS)

APP_TOOLS_MAKE := $(APP_TOOLS_DIR)/$(TOOLTUPLE)-make
APP_TOOLS_CC := $(APP_TOOLS_DIR)/$(TOOLTUPLE)-${_CCVARIANT}
APP_TOOLS_CXX := $(APP_TOOLS_DIR)/$(TOOLTUPLE)-${_CXXVARIANT}

define APPTOOLS_templ
$(APP_TOOLS_DIR)/${2}:							\
    $(APP_TOOLS_DIR)/${1} Makefile
	sed <$$< >$$@.tmp						\
		-e 's#!CC!#$(CC)#g;'					\
		-e 's#!CXX!#$(CXX)#g;'					\
		-e 's#!APPTOOLS_CC!#$(APP_TOOLS_CC)#g;'			\
		-e 's#!APPTOOLS_CXX!#$(APP_TOOLS_CXX)#g;'		\
		-e 's#!MACHINE_ARCH!#$(MACHINE_ARCH)#g;'		\
		-e 's#!GNUPLATFORM!#$(TOOLTUPLE)#g;'			\
		-e 's#!DESTDIR!#$(RRDEST)#g;'				\
		-e 's#!APPTOOLS_DIR!#$(APP_TOOLS_DIR)#g;'		\
		-e 's#!APPTOOLS_PLATFORM!#$(PLATFORM)#g;'		\
		-e 's#!CPPFLAGS!#$(BUILDRUMP_TOOL_CPPFLAGS)#g;'		\
		-e 's#!CFLAGS!#$(BUILDRUMP_TOOL_CFLAGS)#g;'		\
		-e 's#!CXXFLAGS!#$(BUILDRUMP_TOOL_CXXFLAGS)#g;'		\
		-e 's#!LDFLAGS_BAKE!#$(LDFLAGS_BAKE)#g;'
	if test -x $$<; then chmod +x $$@.tmp; fi
	mv -f $$@.tmp $$@
endef
$(foreach tool,${APP_TOOL_FILES},$(eval \
    $(call APPTOOLS_templ,${tool}.in,$(TOOLTUPLE)-${tool})))
$(foreach spec,${APP_TOOL_SPECS},$(eval \
    $(call APPTOOLS_templ,${spec}.in,${spec})))
$(eval $(call APPTOOLS_templ,cc.in,$(TOOLTUPLE)-${_CCVARIANT}))
$(eval $(call APPTOOLS_templ,cc.in,$(TOOLTUPLE)-${_CXXVARIANT}))
$(eval $(call APPTOOLS_templ,specs-bake.in,specs-bake-$(TOOLTUPLE)-$(PLATFORM)))

define APPTOOLS_passthrough
uptool_$1=$(shell echo $1 | tr '[:lower:]' '[:upper:]')
toolval_$1=$${$${uptool_$1}}
$(APP_TOOLS_DIR)/$(TOOLTUPLE)-$1:
	printf '#!/bin/sh\n\nexec %s "$$$${@}"\n' $${toolval_${1}} >$$@.tmp
	chmod +x $$@.tmp
	mv -f $$@.tmp $$@
endef
$(foreach t,${APP_TOOL_PASSTHROUGH},$(eval $(call APPTOOLS_passthrough,${t})))

clean:
	rm -f $(_APP_TOOL_TARGETS)
