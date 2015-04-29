.PHONY: all-tests
all-tests:
	$(MAKE) -C hello
	$(MAKE) -C basic

.PHONY: clean
clean:
	$(MAKE) -C hello clean
	$(MAKE) -C basic clean
	[ ! -f configure/Makefile ] || $(MAKE) -C configure distclean
