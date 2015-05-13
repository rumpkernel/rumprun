/* 
 ****************************************************************************
 * (C) 2005 - Grzegorz Milos - Intel Research Cambridge
 ****************************************************************************
 *
 *        File: sched.c
 *      Author: Grzegorz Milos
 *     Changes: Robert Kaiser
 *              
 *        Date: Aug 2005
 * 
 * Environment: Xen Minimal OS
 * Description: simple scheduler for Mini-Os
 *
 * The scheduler is non-preemptive (cooperative), and schedules according 
 * to Round Robin algorithm.
 *
 ****************************************************************************
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

#include <mini-os/os.h>
#include <mini-os/hypervisor.h>
#include <mini-os/time.h>
#include <mini-os/mm.h>
#include <mini-os/types.h>
#include <mini-os/lib.h>
#include <mini-os/semaphore.h>

#include <bmk-core/memalloc.h>
#include <bmk-core/sched.h>

#if 0
void dump_stack(struct thread *thread_md)
{
    unsigned long *bottom = (unsigned long *)(thread->stack + STACK_SIZE); 
    unsigned long *pointer = (unsigned long *)thread->thr_sp;
    int count;
    if(thread == bmk_current)
    {
#ifdef __i386__    
        asm("movl %%esp,%0"
            : "=r"(pointer));
#else
        asm("movq %%rsp,%0"
            : "=r"(pointer));
#endif
    }
    minios_printk("The stack for \"%s\"\n", thread->name);
    for(count = 0; count < 25 && pointer < bottom; count ++)
    {
        minios_printk("[0x%lx] 0x%lx\n", pointer, *pointer);
        pointer++;
    }
    
    if(pointer < bottom) minios_printk(" ... continues.\n");
}
#endif

void
bmk_platform_cpu_sched_settls(struct bmk_tcb *next)
{

#if defined(__i386__)
    tlsswitch32(next->btcb_tp);
#else /* x86_64 */
    wrmsrl(0xc0000100, next->btcb_tp);
#endif
}
