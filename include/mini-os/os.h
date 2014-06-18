#ifndef _MINIOS_OS_H_
#define _MINIOS_OS_H_

#define smp_processor_id() 0
#define unlikely(x)  __builtin_expect((x),0)
#define likely(x)  __builtin_expect((x),1)

#include <mini-os/hypervisor.h>

#ifndef __RUMP_KERNEL__

#if __GNUC__ == 2 && __GNUC_MINOR__ < 96
#define __builtin_expect(x, expected_value) (x)
#endif

#ifndef __ASSEMBLY__
#include <mini-os/types.h>
#include <mini-os/kernel.h>
#endif

#define USED    __attribute__ ((used))

#define BUG do_exit

#include <mini-os/machine/os.h>

#endif /* !__RUMP_KERNEL__ */

#endif /* _MINIOS_OS_H_ */
