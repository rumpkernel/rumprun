include ../Makefile.inc

ALL=hello.bin

# ifeq ($(CONFIG_CXX),yes)
# ALL += hellopp.bin
# endif

all: $(ALL)

hello: hello.c

hellopp: hellopp.cc

%.bin: %
	$(RUMPBAKE) -o $@ $<

clean:
	rm -f hello hellopp
