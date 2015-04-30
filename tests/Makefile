.PHONY: all-tests
all-tests:
	$(MAKE) -C hello
	$(MAKE) -C basic
	$(MAKE) -C crypto

.PHONY: clean
clean:
	$(MAKE) -C hello clean
	$(MAKE) -C basic clean
	$(MAKE) -C crypto clean
	[ ! -f configure/Makefile ] || $(MAKE) -C configure distclean
