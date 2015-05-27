include ../Makefile.inc

ALL=md5.bin

all: $(ALL)

md5: md5.c
	${CC} ${CPPFLAGS} ${CFLAGS} -o $@ $< -lcrypto ${LDFLAGS}

md5.bin: md5
	$(RUMPBAKE) -o $@ $<

clean:
	rm -f md5
