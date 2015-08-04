#include <bmk-core/printf.h>
#define STUB(name) \
    int name(void); int name(void) {bmk_printf("STUB %s\n", #name);for(;;);}

STUB(cpu_intr_init);
STUB(cpu_intr_ack);
