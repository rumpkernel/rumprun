#include <bmk/kernel.h>
#include <bmk/sched.h>

#define LIBRUMPUSER
#include "rumpuser_int.h"

static struct bmk_thread *isr_thread;
static int (*isr_func[BMK_MAXINTR])(void *);
static void *isr_arg[BMK_MAXINTR];
static unsigned int isr_todo;

void
bmk_isr_clock(void)
{

	/* nada */
}

/* thread context we use to deliver interrupts to the rump kernel */
static void
isr(void *arg)
{
	int rv, i;

        rumpuser__hyp.hyp_schedule();
        rumpuser__hyp.hyp_lwproc_newlwp(0);
        rumpuser__hyp.hyp_unschedule();
	for (;;) {
		splhigh();
		if (isr_todo) {
			unsigned int isrcopy;

			isrcopy = isr_todo;
			isr_todo = 0;
			spl0();

			rv = 0;
			for (i = 0; i < sizeof(isr_todo)*8; i++) {
				if ((isrcopy & (1<<i)) == 0)
					continue;

				rumpuser__hyp.hyp_schedule();
				rv += isr_func[i](isr_arg[i]);
				rumpuser__hyp.hyp_unschedule();
			}

			/*
			 * ACK interrupts on PIC
			 */
			if (rv) {
				__asm__ __volatile(
				    "movb $0x20, %%al\n"
				    "outb %%al, $0xa0\n"
				    "outb %%al, $0x20\n"
				    ::: "al");
			}
		} else {
			/* no interrupts left. block until the next one. */
			bmk_sched_block(isr_thread);
			spl0();
			bmk_sched();
		}
	}
}

int
bmk_isr_netinit(int (*func)(void *), void *arg, int intr)
{
	int error;

	if (intr > sizeof(isr_todo)*8 || intr > BMK_MAXINTR)
		return EGENERIC;

	/* TODO: sharing */
	if (isr_func[intr])
		return EBUSY;

	if ((error = bmk_cpu_intr_init(intr)) != 0)
		return error;
	isr_func[intr] = func;
	isr_arg[intr] = arg;

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
bmk_isr_init(void)
{

	isr_thread = bmk_sched_create("netisr", NULL, 0, isr, NULL, NULL, 0);
	if (!isr_thread)
		return EGENERIC;
	return 0;
}
