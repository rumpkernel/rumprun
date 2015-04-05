int _stub_func(void); int _stub_func(void) {return 0;}

#define STUB(name) \
int name(void) __attribute__((alias("_stub_func")));

#define WEAK_STUB(name) \
int name(void) __attribute__((weak,alias("_stub_func")));

STUB(__errno);
STUB(_exit);
STUB(_lwp_kill);
STUB(_lwp_self);
STUB(_mmap);
STUB(calloc);
STUB(free);
STUB(malloc);
STUB(madvise);
STUB(minherit);
STUB(mmap);
STUB(mprotect);
STUB(munmap);
STUB(realloc);

STUB(__sigaction14);
WEAK_STUB(__sigprocmask14);
STUB(sigprocmask);
STUB(sigaction);
STUB(__sigaction_sigtramp);
STUB(__getrusage50);

WEAK_STUB(fcntl)

STUB(__fork);
STUB(__vfork14);
STUB(execve);
STUB(kill);
STUB(getpriority);
STUB(setpriority);

/* for pthread_cancelstub */
STUB(_sys_mq_send);
STUB(_sys_mq_receive);
STUB(_sys___mq_timedsend50);
STUB(_sys___mq_timedreceive50);
STUB(_sys_msgrcv);
STUB(_sys_msgsnd);
STUB(_sys___msync13);
STUB(_sys___wait450);
STUB(_sys___sigsuspend14);
STUB(_sys___sigprocmask14);
STUB(____sigtimedwait50);

STUB(rumpuser_anonmmap);
STUB(rumpuser_clock_gettime);
STUB(rumpuser_clock_sleep);
STUB(rumpuser_curlwp);
STUB(rumpuser_curlwpop);
STUB(rumpuser_cv_broadcast);
STUB(rumpuser_cv_destroy);
STUB(rumpuser_cv_has_waiters);
STUB(rumpuser_cv_init);
STUB(rumpuser_cv_signal);
STUB(rumpuser_cv_timedwait);
STUB(rumpuser_cv_wait);
STUB(rumpuser_cv_wait_nowrap);
STUB(rumpuser_daemonize_begin);
STUB(rumpuser_daemonize_done);
STUB(rumpuser_dl_bootstrap);
STUB(rumpuser_dprintf);
STUB(rumpuser_exit);
STUB(rumpuser_free);
STUB(rumpuser_getparam);
STUB(rumpuser_getrandom);
STUB(rumpuser_init);
STUB(rumpuser_iovread);
STUB(rumpuser_iovwrite);
STUB(rumpuser_kill);
STUB(rumpuser_malloc);
STUB(rumpuser_mutex_destroy);
STUB(rumpuser_mutex_enter);
STUB(rumpuser_mutex_enter_nowrap);
STUB(rumpuser_mutex_exit);
STUB(rumpuser_mutex_init);
STUB(rumpuser_mutex_owner);
STUB(rumpuser_mutex_tryenter);
STUB(rumpuser_putchar);
STUB(rumpuser_rw_destroy);
STUB(rumpuser_rw_downgrade);
STUB(rumpuser_rw_enter);
STUB(rumpuser_rw_exit);
STUB(rumpuser_rw_held);
STUB(rumpuser_rw_init);
STUB(rumpuser_rw_tryenter);
STUB(rumpuser_rw_tryupgrade);
STUB(rumpuser_seterrno);
STUB(rumpuser_thread_create);
STUB(rumpuser_thread_exit);
STUB(rumpuser_thread_join);
STUB(rumpuser_unmap);

STUB(_lwp_rumpxen_gettcb);
STUB(_lwp_ctl);
STUB(_lwp_wakeup);
STUB(_lwp_setname);
STUB(_lwp_continue);
STUB(_lwp_suspend);
STUB(_lwp_exit);
STUB(___lwp_park60);
STUB(_lwp_unpark_all);
STUB(_lwp_unpark);

STUB(_rtld_tls_free);
STUB(_rtld_tls_allocate);
STUB(_sys_setcontext);
STUB(_sched_getparam);
STUB(_sched_setparam);
STUB(_sched_getaffinity);
STUB(_sys_sched_yield);
STUB(sched_yield);
STUB(rasctl);
STUB(__libc_static_tls_setup);
STUB(rumprun_makelwp);
STUB(_sched_setaffinity);

STUB(posix_spawn);
