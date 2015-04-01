#ifndef _BMK_CORE_SCHED_H_
#define _BMK_CORE_SCHED_H_

struct bmk_tcb {
	unsigned long btcb_sp;		/* stack pointer	*/
	unsigned long btcb_ip;		/* program counter	*/

	unsigned long btcb_tp;		/* tls pointer		*/
	unsigned long btcb_tpsize;	/* tls area length	*/
};

#endif /* _BMK_CORE_SCHED_H_ */
