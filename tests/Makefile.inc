CFLAGS+= -Wall -Werror -Wmissing-prototypes -Wstrict-prototypes -g
LDLIBS= -lrumprun_tester

%:: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

%:: %.cc
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDLIBS)

%.bin: %
	$(RUMPBAKE) -o $@ $<
