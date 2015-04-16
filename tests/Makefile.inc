CFLAGS+= -Wall -Werror -Wmissing-prototypes -Wstrict-prototypes -g
ifeq (${RUMPRUN_TEST},no)
CPPFLAGS+= -DRUMPRUN_TESTER_JUSTMAIN
else
LDFLAGS= -lrumprun_tester
endif
