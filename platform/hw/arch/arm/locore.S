#include <hw/kernel.h>

ENTRY(_start)
loop:
	b loop
END(_start)
