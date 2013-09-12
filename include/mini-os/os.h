#ifndef _MINIOS_OS_H_
#define _MINIOS_OS_H_

#if __GNUC__ == 2 && __GNUC_MINOR__ < 96
#define __builtin_expect(x, expected_value) (x)
#endif
#define unlikely(x)  __builtin_expect((x),0)
#define likely(x)  __builtin_expect((x),1)

#define smp_processor_id() 0

#ifndef __ASSEMBLY__
#include <mini-os/types.h>
#include <mini-os/hypervisor.h>
#include <mini-os/kernel.h>
#endif

#define USED    __attribute__ ((used))

#define BUG do_exit

#include <mini-os/machine/os.h>

#endif /* _MINIOS_OS_H_ */
