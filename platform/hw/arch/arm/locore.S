#include <bmk/kernel.h>

ENTRY(_start)
loop:
	b loop
END(_start)
