/*-
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

#include <bmk-core/i386/asm.h>

ENTRY(bmk_cpu_sched_bouncer)
	popl %eax
	popl %ebx
	pushl $0
	xorl %ebp,%ebp
	pushl %eax
	call *%ebx
	call bmk_sched_exit
END(bmk_cpu_sched_bouncer)

ENTRY(bmk_cpu_sched_switch)
	movl 4(%esp), %ecx		/* prev */
	movl 8(%esp), %edx		/* next */

	pushl %ebp
	pushl %ebx
	pushl %esi
	pushl %edi
	movl %esp, (%ecx)		/* save ESP */
	movl (%edx), %esp		/* restore ESP */
	movl $1f, 4(%ecx)		/* save EIP */
	pushl 4(%edx)			/* restore EIP */
	ret
1:
	popl %edi
	popl %esi
	popl %ebx
	popl %ebp
	ret
END(bmk_cpu_sched_switch)
