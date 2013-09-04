CFLAGS=	-O2 -g -Wall

CFLAGS+= -nostdinc -I../rump/include -I../include
CFLAGS+= -DNO_DEBUG -DNO_USER_SUPPORT -DNO_CGIBIN_SUPPORT -DNO_DAEMON_MODE
CFLAGS+= -DNO_DYNAMIC_CONTENT -DNO_SSL_SUPPORT

FILES=	bozohttpd.o auth-bozo.o cgi-bozo.o content-bozo.o daemon-bozo.o \
	dir-index-bozo.o ssl-bozo.o tilde-luzah-bozo.o main.o

# note: no linking
all: $(FILES)

clean:
	rm -f .dummy *.o
