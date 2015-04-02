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
#include <mini-os/sched.h>
#include <mini-os/semaphore.h>

#include <bmk-core/memalloc.h>
#include <bmk-core/sched.h>

#if 0
void dump_stack(struct thread *thread_md)
{
    unsigned long *bottom = (unsigned long *)(thread->stack + STACK_SIZE); 
    unsigned long *pointer = (unsigned long *)thread->thr_sp;
    int count;
    if(thread == get_current())
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

/* Gets run when a new thread is scheduled the first time ever, 
   defined in x86_[32/64].S */
extern void _minios_entry_thread_starter(void);

/* Pushes the specified value onto the stack of the specified thread */
static void stack_push(struct bmk_tcb *tcb, unsigned long value)
{
    tcb->btcb_sp -= sizeof(unsigned long);
    *((unsigned long *)tcb->btcb_sp) = value;
}

/* Architecture specific setup of thread creation */
void arch_create_thread(void *thread, struct bmk_tcb *tcb,
	void (*function)(void *), void *data,
	void *stack, unsigned long stack_size)
{
    
    tcb->btcb_sp = (unsigned long)stack + stack_size;
    /* Save pointer to the thread on the stack, used by current macro */
    *((unsigned long *)stack) = (unsigned long)thread;
    
    stack_push(tcb, (unsigned long) function);
    stack_push(tcb, (unsigned long) data);
    tcb->btcb_ip = (unsigned long) _minios_entry_thread_starter;
}

void run_idle_thread(void)
{
    /* Switch stacks and run the thread */ 
#if defined(__i386__)
    __asm__ __volatile__("mov %0,%%esp\n\t"
                         "push %1\n\t" 
                         "ret"                                            
                         :"=m" (idle_tcb->btcb_sp)
                         :"m" (idle_tcb->btcb_ip));
#elif defined(__x86_64__)
    __asm__ __volatile__("mov %0,%%rsp\n\t"
                         "push %1\n\t" 
                         "ret"                                            
                         :"=m" (idle_tcb->btcb_sp)
                         :"m" (idle_tcb->btcb_ip));
#endif
}

void arch__switch(unsigned long *, unsigned long *);
void
arch_switch_threads(struct bmk_tcb *prev, struct bmk_tcb *next)
{

/* XXX: TLS is available only on x86_64 currently */
#if defined(__x86_64__)
    wrmsrl(0xc0000100, next->btcb_tp);
#endif

    arch__switch(&prev->btcb_sp, &next->btcb_sp);
}
