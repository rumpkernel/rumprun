/*-
 * Copyright (c) 2014 Antti Kantee.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <bmk/kernel.h>

#include <bmk-core/core.h>
#include <bmk-core/memalloc.h>
#include <bmk-core/printf.h>
#include <bmk-core/queue.h>
#include <bmk-core/sched.h>

#include <bmk-rumpuser/rumpuser.h>

struct intrhand {
	int (*ih_fun)(void *);
	void *ih_arg;

	SLIST_ENTRY(intrhand) ih_entries;
};

SLIST_HEAD(isr_ihead, intrhand);
static struct isr_ihead isr_ih[BMK_MAXINTR];
static unsigned int isr_todo;
static unsigned int isr_lowest = sizeof(isr_todo)*8;

static struct bmk_thread *isr_thread;

void
bmk_isr_clock(void)
{

	/* nada */
}

/* thread context we use to deliver interrupts to the rump kernel */
static void
isr(void *arg)
{
	int i, didwork;

        rumpuser__hyp.hyp_schedule();
        rumpuser__hyp.hyp_lwproc_newlwp(0);
        rumpuser__hyp.hyp_unschedule();

	didwork = 0;
	for (;;) {
		splhigh();
		if (isr_todo) {
			unsigned int isrcopy;
			int nlocks = 1;

			isrcopy = isr_todo;
			isr_todo = 0;
			spl0();

			rumpkern_sched(nlocks, NULL);
			for (i = isr_lowest; isrcopy; i++) {
				struct intrhand *ih;

				bmk_assert(i < sizeof(isrcopy)*8);

				if ((isrcopy & (1<<i)) == 0)
					continue;
				isrcopy &= ~(1<<i);

				SLIST_FOREACH(ih, &isr_ih[i], ih_entries) {
					if (ih->ih_fun(ih->ih_arg) != 0) {
						didwork = 1;
					}
				}
			}
			rumpkern_unsched(&nlocks, NULL);
			bmk_cpu_intr_ack();

			if (!didwork) {
				bmk_printf("stray interrupt\n");
			}
		} else {
			/* no interrupts left. block until the next one. */
			bmk_sched_blockprepare();
			spl0();
			bmk_sched_block();
			didwork = 0;
		}
	}
}

int
bmk_isr_init(int (*func)(void *), void *arg, int intr)
{
	struct intrhand *ih;
	int error;

	if (intr > sizeof(isr_todo)*8 || intr > BMK_MAXINTR)
		return BMK_EGENERIC;

	ih = bmk_xmalloc_bmk(sizeof(*ih));
	if (!ih)
		return BMK_ENOMEM;

	if ((error = bmk_cpu_intr_init(intr)) != 0) {
		bmk_memfree(ih, BMK_MEMWHO_WIREDBMK);
		return error;
	}
	ih->ih_fun = func;
	ih->ih_arg = arg;
	SLIST_INSERT_HEAD(&isr_ih[intr], ih, ih_entries);
	if ((unsigned)intr < isr_lowest)
		isr_lowest = intr;

	return 0;
}

void
bmk_isr(int which)
{

	/* schedule the interrupt handler */
	isr_todo |= 1<<which;
	bmk_sched_wake(isr_thread);
}

int
bmk_intr_init(void)
{
	int i;

	for (i = 0; i < BMK_MAXINTR; i++) {
		SLIST_INIT(&isr_ih[i]);
	}

	isr_thread = bmk_sched_create("isrthr", NULL, 0, isr, NULL, NULL, 0);
	if (!isr_thread)
		return BMK_EGENERIC;
	return 0;
}
