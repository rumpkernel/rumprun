include ../Makefile.inc

ALL=md5

all: $(ALL)

md5: md5.c
	${CC} ${CFLAGS} -o $@ $< -lcrypto ${LDFLAGS}

clean:
	rm -f md5
