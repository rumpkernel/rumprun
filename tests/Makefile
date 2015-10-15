.PHONY: all-tests
all-tests:
	$(MAKE) -C hello
	$(MAKE) -C basic
	$(MAKE) -C crypto

.PHONY: kernonly-tests
kernonly-tests:
	$(MAKE) PLATFORM=${PLATFORM} -C nolibc

.PHONY: clean
clean:
	$(MAKE) -C hello clean
	$(MAKE) -C basic clean
	$(MAKE) -C crypto clean
	$(MAKE) -C nolibc clean
	[ ! -f configure/Makefile ] || $(MAKE) -C configure distclean
