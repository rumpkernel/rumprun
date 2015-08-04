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

#include <bmk-core/core.h>
#include <bmk-core/sched.h>

static void
stack_push(void **stackp, unsigned long value)
{
	unsigned long *stack = *stackp;

	stack--;
	*stack = value;
	*stackp = stack;
}

void
bmk_cpu_sched_create(struct bmk_thread *thread, struct bmk_tcb *tcb,
	void (*f)(void *), void *arg,
	void *stack_base, unsigned long stack_size)
{
	void *stack_top = (char *)stack_base + stack_size;

	/* Save pointer to the thread on the stack, used by current macro */
	*(unsigned long *)stack_base = (unsigned long)thread;

	/* these values are used by bmk_cpu_sched_bouncer() */
	stack_push(&stack_top, (unsigned long)f);
	stack_push(&stack_top, (unsigned long)arg);

	tcb->btcb_sp = (unsigned long)stack_top;
	tcb->btcb_ip = (unsigned long)bmk_cpu_sched_bouncer;
}
