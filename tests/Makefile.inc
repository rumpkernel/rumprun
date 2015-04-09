CFLAGS+= -Wall -Werror -Wmissing-prototypes -Wstrict-prototypes
ifeq (${RUMPRUN_TEST},no)
CPPFLAGS+= -DRUMPRUN_TESTER_JUSTMAIN
else
LDFLAGS= -lrumprun_tester
endif
