/*-
 * Copyright (c) 2015 Antti Kantee.  All Rights Reserved.
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

#ifndef _BMK_CORE_SCHED_H_
#define _BMK_CORE_SCHED_H_

#include <bmk-core/types.h>

struct bmk_tcb {
	unsigned long btcb_sp;		/* stack pointer	*/
	unsigned long btcb_ip;		/* program counter	*/

	unsigned long btcb_tp;		/* tls pointer		*/
	unsigned long btcb_tpsize;	/* tls area length	*/
};

struct bmk_thread;

void	bmk_sched_init(void);
void	bmk_sched(void);

struct bmk_thread *bmk_sched_create(const char *, void *, int,
				    void (*)(void *), void *,
				    void *, unsigned long);
void	bmk_sched_join(struct bmk_thread *);
void	bmk_sched_exit(void) __attribute__((__noreturn__));

void	bmk_sched_block(struct bmk_thread *);
void	bmk_sched_block_timeout(struct bmk_thread *, bmk_time_t);

void	bmk_sched_wake(struct bmk_thread *);
void	bmk_sched_setwakeup(struct bmk_thread *, bmk_time_t);

int	bmk_sched_nanosleep(bmk_time_t);
int	bmk_sched_nanosleep_abstime(bmk_time_t);

void	*bmk_sched_gettls(struct bmk_thread *, unsigned int);
void	bmk_sched_settls(struct bmk_thread *, unsigned int, void *);

void	bmk_cpu_sched_create(struct bmk_tcb *,
			     void (*)(void *), void *,
			     void *, unsigned long);
void	bmk_cpu_sched_switch(struct bmk_tcb *, struct bmk_tcb *);

void	bmk_sched_set_hook(void (*)(void *, void *));
struct bmk_thread *bmk_sched_init_mainlwp(void *);

struct bmk_thread *bmk_sched_current(void);
int *bmk_sched_geterrno(void);

bmk_time_t	bmk_clock_monotonic(void);

/* XXX: coming up with better names considered useful */
void	bmk_cpu_sched_bouncer(void);
void	bmk_cpu_switch(struct bmk_tcb *, struct bmk_tcb *);
void	bmk__cpu_switch(void *, void *);

#endif /* _BMK_CORE_SCHED_H_ */
