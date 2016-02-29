include ${RUMPRUN_MKCONF}

CC= ${RUMPRUN_CC}
CXX= ${RUMPRUN_CXX}

CFLAGS+= -Wall -Werror -Wmissing-prototypes -Wstrict-prototypes -g
LDLIBS= -lrumprun_tester

%:: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

%:: %.cc
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDLIBS)

%.bin: %
	$(RUMPRUN_BAKE) $(RUMPBAKE_PLATFORM) $@ $<
