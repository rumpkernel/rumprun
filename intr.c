#include <bmk/kernel.h>
#include <bmk/sched.h>

#define LIBRUMPUSER
#include "rumpuser_int.h"

void
bmk_isr_clock(void)
{

	/* nada */
}

/*
 * yeayeayea, this needs a bit of improving
 */

static struct bmk_thread *netisr_thread;
static int (*netisr_func)(void *);
static void *netisr_arg;
static int netisr_todo;

/* thread context we use to deliver interrupts to the rump kernel */
static void
netisr(void *arg)
{
	int rv;

        rumpuser__hyp.hyp_schedule();
        rumpuser__hyp.hyp_lwproc_newlwp(0);
        rumpuser__hyp.hyp_unschedule();
	for (;;) {
		splhigh();
		if (netisr_todo) {
			netisr_todo = 0;
			spl0();
			rumpuser__hyp.hyp_schedule();
			rv = netisr_func(netisr_arg);
			rumpuser__hyp.hyp_unschedule();

			/*
			 * ACK the interrupt if we've really processed it
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
			bmk_sched_block(netisr_thread);
			spl0();
			bmk_sched();
		}
	}
}

int
bmk_isr_netinit(int (*func)(void *), void *arg, int intr)
{
	int error;

	if ((error = bmk_cpu_intr_init(intr)) != 0)
		return error;
	netisr_thread = bmk_sched_create("netisr", NULL, 0,
	    netisr, NULL, NULL, 0);
	if (!netisr_thread)
		return EGENERIC;
	netisr_func = func;
	netisr_arg = arg;

	return 0;
}

void
bmk_isr(int which)
{

	/* schedule the interrupt handler */
	netisr_todo = 1;
	bmk_sched_wake(netisr_thread);
}
