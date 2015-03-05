/* from Xen MiniOS, with following copyright */

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

#include <bmk/types.h>
#include <bmk/sched.h>

static void
stack_push(void **stackp, unsigned long value)
{
	unsigned long *stack = *stackp;

	stack--;
	*stack = value;
	*stackp = stack;
}

void bmk_cpu_sched_starter(void);
void
bmk_cpu_sched_create(struct bmk_thread *thread,
	void (*f)(void *), void *arg, void **stackp)
{

	/* these values are used by bmk_cpu_sched_bouncer() */
	stack_push(stackp, (unsigned long)f);
	stack_push(stackp, (unsigned long)arg);
}
