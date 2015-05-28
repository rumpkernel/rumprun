ifeq (${PLATFORM},)
$(error need to specify $$PLATFORM!)
endif

COREDIR:=       $(shell pwd)/../../lib/libbmk_core
RUMPUSERDIR:=   $(shell pwd)/../../lib/libbmk_rumpuser
BASEDIR:=       $(shell pwd)/../../lib/librumprun_base

.PHONY: ${BASEDIR}/${PLATFORM}/librumprun_base.a
${BASEDIR}/${PLATFORM}/librumprun_base.a:
	( cd ${BASEDIR} \
	    && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} obj \
	    && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} dependall )

.PHONY: ${BASEDIR}/${PLATFORM}/librumprun_base_stubs.o
${BASEDIR}/${PLATFORM}/librumprun_base_stubs.o: ${BASEDIR}/librumprun_base_stubs.c
	$(CC) $(CFLAGS) -c -o $@ $<

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

commonlibs: ${BASEDIR}/${PLATFORM}/librumprun_base.a ${BASEDIR}/${PLATFORM}/librumprun_base_stubs.o ${COREDIR}/${PLATFORM}/libbmk_core.a ${RUMPUSERDIR}/${PLATFORM}/libbmk_rumpuser.a
