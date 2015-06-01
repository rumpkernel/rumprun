ifeq (${PLATFORM},)
$(error need to specify $$PLATFORM!)
endif

COREDIR:=       $(shell pwd)/../../lib/libbmk_core
RUMPUSERDIR:=   $(shell pwd)/../../lib/libbmk_rumpuser
BASEDIR:=       $(shell pwd)/../../lib/librumprun_base

RUMPKERN_LIB:=	-lrump

BASELIBDIRS:=	-L${BASEDIR}/${PLATFORM} -L${COREDIR}/${PLATFORM}	\
		-L${RUMPUSERDIR}/${PLATFORM}

ifeq (${CONFIG_SYSPROXY},yes)
LIBS_SYSPROXY=  -lrumpkern_sysproxy
endif

LIBS_USER:=	${BASELIBDIRS}						\
		--start-group -lrumprun_base -lpthread -lc --end-group	\
		-lbmk_rumpuser -lbmk_core
LIBS_USERSTUB:=	${BASELIBDIRS}						\
		--start-group -lpthread -lc --end-group			\
		-lbmk_core ${BASEDIR}/${PLATFORM}/librumprun_base_stubs.o

RUMP_LDLIBS:=	--whole-archive ${LIBS_SYSPROXY} ${RUMPKERN_LIB}	\
		--no-whole-archive ${LIBS_USER}
RUMP_STUBLDLIBS:=--whole-archive ${RUMPKERN_LIB} --no-whole-archive	\
		${LIBS_USERSTUB}

.PHONY: ${BASEDIR}/${PLATFORM}/librumprun_base.a
${BASEDIR}/${PLATFORM}/librumprun_base.a:
	( cd ${BASEDIR} \
	    && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} obj \
	    && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} dependall )

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

.PHONY: commonclean
commonclean:
	( cd ${BASEDIR} && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} cleandir )
	( cd ${COREDIR} && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} cleandir )
	( cd ${RUMPUSERDIR} && ${RUMPMAKE} MAKEOBJDIR=${PLATFORM} cleandir )
