ALL=hello

ifeq ($(CONFIG_CXX),yes)
ALL += hellopp
endif

all: $(ALL)

hello: hello.c

hellopp: hellopp.cc

clean:
	rm -f hello hellopp
