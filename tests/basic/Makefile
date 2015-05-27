include ../Makefile.inc

ALL=tls_test.bin ctor_test.bin pthread_test.bin misc_test.bin

all: $(ALL)

%: %.c

%.bin: %
	$(RUMPBAKE) -o $@ $<

clean:
	rm -f $(ALL)
