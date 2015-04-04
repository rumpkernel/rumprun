#ifndef _BMK_CORE_SCHED_H_
#define _BMK_CORE_SCHED_H_

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

#endif /* _BMK_CORE_SCHED_H_ */
