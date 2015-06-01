/* TODO: This list of stubs is somewhat haphazardly copied from the old
 * platform/xen/configure_stubs.c. We should be able to generate most of this
 * using the symbol tables in the libraries we're stubbing out. */

int _stub_func(void); int _stub_func(void) {return 0;}

#define STUB(name) \
int name(void) __attribute__((alias("_stub_func")));

#define WEAK_STUB(name) \
int name(void) __attribute__((weak,alias("_stub_func")));

STUB(_start);

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
STUB(mincore);
STUB(mlockall);
STUB(posix_memalign);

STUB(__sigaction14);
WEAK_STUB(__sigprocmask14);
STUB(sigprocmask);
STUB(sigaction);
STUB(sigpending);
STUB(__sigaction_sigtramp);
STUB(__getrusage50);
STUB(__sigpending14);

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

STUB(_lwp_ctl);
STUB(_lwp_wakeup);
STUB(_lwp_setname);
STUB(_lwp_continue);
STUB(_lwp_suspend);
STUB(_lwp_exit);
STUB(___lwp_park60);
STUB(_lwp_unpark_all);
STUB(_lwp_unpark);
STUB(_lwp_getprivate);

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

STUB(rumpuser__hyp);
STUB(rumprun);
STUB(rumprun_boot);
STUB(rumprun_reboot);
STUB(rumprun_wait);
STUB(rumprun_cmdline_argv);
STUB(rumprun_cmdline_argc);

STUB(__dso_handle);
/* AUTOMATICALLY GENERATED */

#include <sys/cdefs.h>

#include <bmk-core/errno.h>

int _stubnosys(void); int _stubnosys(void) {return BMK_ENOSYS;}

__weak_alias(read,_stubnosys);
__weak_alias(_read,_stubnosys);
__strong_alias(_sys_read,_stubnosys);

__weak_alias(write,_stubnosys);
__weak_alias(_write,_stubnosys);
__strong_alias(_sys_write,_stubnosys);

__weak_alias(open,_stubnosys);
__weak_alias(_open,_stubnosys);
__strong_alias(_sys_open,_stubnosys);

__weak_alias(close,_stubnosys);
__weak_alias(_close,_stubnosys);
__strong_alias(_sys_close,_stubnosys);

__weak_alias(link,_stubnosys);
__weak_alias(_link,_stubnosys);
__strong_alias(_sys_link,_stubnosys);

__weak_alias(unlink,_stubnosys);
__weak_alias(_unlink,_stubnosys);
__strong_alias(_sys_unlink,_stubnosys);

__weak_alias(chdir,_stubnosys);
__weak_alias(_chdir,_stubnosys);
__strong_alias(_sys_chdir,_stubnosys);

__weak_alias(fchdir,_stubnosys);
__weak_alias(_fchdir,_stubnosys);
__strong_alias(_sys_fchdir,_stubnosys);

__weak_alias(mknod,_stubnosys);
__weak_alias(_mknod,_stubnosys);
__strong_alias(_sys_mknod,_stubnosys);

__weak_alias(chmod,_stubnosys);
__weak_alias(_chmod,_stubnosys);
__strong_alias(_sys_chmod,_stubnosys);

__weak_alias(chown,_stubnosys);
__weak_alias(_chown,_stubnosys);
__strong_alias(_sys_chown,_stubnosys);

__weak_alias(getpid,_stubnosys);
__weak_alias(_getpid,_stubnosys);
__strong_alias(_sys_getpid,_stubnosys);

__weak_alias(unmount,_stubnosys);
__weak_alias(_unmount,_stubnosys);
__strong_alias(_sys_unmount,_stubnosys);

__weak_alias(setuid,_stubnosys);
__weak_alias(_setuid,_stubnosys);
__strong_alias(_sys_setuid,_stubnosys);

__weak_alias(getuid,_stubnosys);
__weak_alias(_getuid,_stubnosys);
__strong_alias(_sys_getuid,_stubnosys);

__weak_alias(geteuid,_stubnosys);
__weak_alias(_geteuid,_stubnosys);
__strong_alias(_sys_geteuid,_stubnosys);

__weak_alias(recvmsg,_stubnosys);
__weak_alias(_recvmsg,_stubnosys);
__strong_alias(_sys_recvmsg,_stubnosys);

__weak_alias(sendmsg,_stubnosys);
__weak_alias(_sendmsg,_stubnosys);
__strong_alias(_sys_sendmsg,_stubnosys);

__weak_alias(recvfrom,_stubnosys);
__weak_alias(_recvfrom,_stubnosys);
__strong_alias(_sys_recvfrom,_stubnosys);

__weak_alias(accept,_stubnosys);
__weak_alias(_accept,_stubnosys);
__strong_alias(_sys_accept,_stubnosys);

__weak_alias(getpeername,_stubnosys);
__weak_alias(_getpeername,_stubnosys);
__strong_alias(_sys_getpeername,_stubnosys);

__weak_alias(getsockname,_stubnosys);
__weak_alias(_getsockname,_stubnosys);
__strong_alias(_sys_getsockname,_stubnosys);

__weak_alias(access,_stubnosys);
__weak_alias(_access,_stubnosys);
__strong_alias(_sys_access,_stubnosys);

__weak_alias(chflags,_stubnosys);
__weak_alias(_chflags,_stubnosys);
__strong_alias(_sys_chflags,_stubnosys);

__weak_alias(fchflags,_stubnosys);
__weak_alias(_fchflags,_stubnosys);
__strong_alias(_sys_fchflags,_stubnosys);

__weak_alias(sync,_stubnosys);
__weak_alias(_sync,_stubnosys);
__strong_alias(_sys_sync,_stubnosys);

__weak_alias(getppid,_stubnosys);
__weak_alias(_getppid,_stubnosys);
__strong_alias(_sys_getppid,_stubnosys);

__weak_alias(dup,_stubnosys);
__weak_alias(_dup,_stubnosys);
__strong_alias(_sys_dup,_stubnosys);

__weak_alias(pipe,_stubnosys);
__weak_alias(_pipe,_stubnosys);
__strong_alias(_sys_pipe,_stubnosys);

__weak_alias(getegid,_stubnosys);
__weak_alias(_getegid,_stubnosys);
__strong_alias(_sys_getegid,_stubnosys);

__weak_alias(ktrace,_stubnosys);
__weak_alias(_ktrace,_stubnosys);
__strong_alias(_sys_ktrace,_stubnosys);

__weak_alias(getgid,_stubnosys);
__weak_alias(_getgid,_stubnosys);
__strong_alias(_sys_getgid,_stubnosys);

__weak_alias(__getlogin,_stubnosys);
__weak_alias(___getlogin,_stubnosys);
__strong_alias(_sys___getlogin,_stubnosys);

__weak_alias(__setlogin,_stubnosys);
__weak_alias(___setlogin,_stubnosys);
__strong_alias(_sys___setlogin,_stubnosys);

__weak_alias(ioctl,_stubnosys);
__weak_alias(_ioctl,_stubnosys);
__strong_alias(_sys_ioctl,_stubnosys);

__weak_alias(revoke,_stubnosys);
__weak_alias(_revoke,_stubnosys);
__strong_alias(_sys_revoke,_stubnosys);

__weak_alias(symlink,_stubnosys);
__weak_alias(_symlink,_stubnosys);
__strong_alias(_sys_symlink,_stubnosys);

__weak_alias(readlink,_stubnosys);
__weak_alias(_readlink,_stubnosys);
__strong_alias(_sys_readlink,_stubnosys);

__weak_alias(umask,_stubnosys);
__weak_alias(_umask,_stubnosys);
__strong_alias(_sys_umask,_stubnosys);

__weak_alias(chroot,_stubnosys);
__weak_alias(_chroot,_stubnosys);
__strong_alias(_sys_chroot,_stubnosys);

__weak_alias(getgroups,_stubnosys);
__weak_alias(_getgroups,_stubnosys);
__strong_alias(_sys_getgroups,_stubnosys);

__weak_alias(setgroups,_stubnosys);
__weak_alias(_setgroups,_stubnosys);
__strong_alias(_sys_setgroups,_stubnosys);

__weak_alias(getpgrp,_stubnosys);
__weak_alias(_getpgrp,_stubnosys);
__strong_alias(_sys_getpgrp,_stubnosys);

__weak_alias(setpgid,_stubnosys);
__weak_alias(_setpgid,_stubnosys);
__strong_alias(_sys_setpgid,_stubnosys);

__weak_alias(dup2,_stubnosys);
__weak_alias(_dup2,_stubnosys);
__strong_alias(_sys_dup2,_stubnosys);

__weak_alias(fcntl,_stubnosys);
__weak_alias(_fcntl,_stubnosys);
__strong_alias(_sys_fcntl,_stubnosys);

__weak_alias(select,_stubnosys);
__weak_alias(_select,_stubnosys);
__strong_alias(_sys_select,_stubnosys);

__weak_alias(fsync,_stubnosys);
__weak_alias(_fsync,_stubnosys);
__strong_alias(_sys_fsync,_stubnosys);

__weak_alias(connect,_stubnosys);
__weak_alias(_connect,_stubnosys);
__strong_alias(_sys_connect,_stubnosys);

__weak_alias(bind,_stubnosys);
__weak_alias(_bind,_stubnosys);
__strong_alias(_sys_bind,_stubnosys);

__weak_alias(setsockopt,_stubnosys);
__weak_alias(_setsockopt,_stubnosys);
__strong_alias(_sys_setsockopt,_stubnosys);

__weak_alias(listen,_stubnosys);
__weak_alias(_listen,_stubnosys);
__strong_alias(_sys_listen,_stubnosys);

__weak_alias(getsockopt,_stubnosys);
__weak_alias(_getsockopt,_stubnosys);
__strong_alias(_sys_getsockopt,_stubnosys);

__weak_alias(readv,_stubnosys);
__weak_alias(_readv,_stubnosys);
__strong_alias(_sys_readv,_stubnosys);

__weak_alias(writev,_stubnosys);
__weak_alias(_writev,_stubnosys);
__strong_alias(_sys_writev,_stubnosys);

__weak_alias(fchown,_stubnosys);
__weak_alias(_fchown,_stubnosys);
__strong_alias(_sys_fchown,_stubnosys);

__weak_alias(fchmod,_stubnosys);
__weak_alias(_fchmod,_stubnosys);
__strong_alias(_sys_fchmod,_stubnosys);

__weak_alias(setreuid,_stubnosys);
__weak_alias(_setreuid,_stubnosys);
__strong_alias(_sys_setreuid,_stubnosys);

__weak_alias(setregid,_stubnosys);
__weak_alias(_setregid,_stubnosys);
__strong_alias(_sys_setregid,_stubnosys);

__weak_alias(rename,_stubnosys);
__weak_alias(_rename,_stubnosys);
__strong_alias(_sys_rename,_stubnosys);

__weak_alias(flock,_stubnosys);
__weak_alias(_flock,_stubnosys);
__strong_alias(_sys_flock,_stubnosys);

__weak_alias(mkfifo,_stubnosys);
__weak_alias(_mkfifo,_stubnosys);
__strong_alias(_sys_mkfifo,_stubnosys);

__weak_alias(sendto,_stubnosys);
__weak_alias(_sendto,_stubnosys);
__strong_alias(_sys_sendto,_stubnosys);

__weak_alias(shutdown,_stubnosys);
__weak_alias(_shutdown,_stubnosys);
__strong_alias(_sys_shutdown,_stubnosys);

__weak_alias(socketpair,_stubnosys);
__weak_alias(_socketpair,_stubnosys);
__strong_alias(_sys_socketpair,_stubnosys);

__weak_alias(mkdir,_stubnosys);
__weak_alias(_mkdir,_stubnosys);
__strong_alias(_sys_mkdir,_stubnosys);

__weak_alias(rmdir,_stubnosys);
__weak_alias(_rmdir,_stubnosys);
__strong_alias(_sys_rmdir,_stubnosys);

__weak_alias(utimes,_stubnosys);
__weak_alias(_utimes,_stubnosys);
__strong_alias(_sys_utimes,_stubnosys);

__weak_alias(setsid,_stubnosys);
__weak_alias(_setsid,_stubnosys);
__strong_alias(_sys_setsid,_stubnosys);

__weak_alias(nfssvc,_stubnosys);
__weak_alias(_nfssvc,_stubnosys);
__strong_alias(_sys_nfssvc,_stubnosys);

__weak_alias(pread,_stubnosys);
__weak_alias(_pread,_stubnosys);
__strong_alias(_sys_pread,_stubnosys);

__weak_alias(pwrite,_stubnosys);
__weak_alias(_pwrite,_stubnosys);
__strong_alias(_sys_pwrite,_stubnosys);

__weak_alias(setgid,_stubnosys);
__weak_alias(_setgid,_stubnosys);
__strong_alias(_sys_setgid,_stubnosys);

__weak_alias(setegid,_stubnosys);
__weak_alias(_setegid,_stubnosys);
__strong_alias(_sys_setegid,_stubnosys);

__weak_alias(seteuid,_stubnosys);
__weak_alias(_seteuid,_stubnosys);
__strong_alias(_sys_seteuid,_stubnosys);

__weak_alias(pathconf,_stubnosys);
__weak_alias(_pathconf,_stubnosys);
__strong_alias(_sys_pathconf,_stubnosys);

__weak_alias(fpathconf,_stubnosys);
__weak_alias(_fpathconf,_stubnosys);
__strong_alias(_sys_fpathconf,_stubnosys);

__weak_alias(getrlimit,_stubnosys);
__weak_alias(_getrlimit,_stubnosys);
__strong_alias(_sys_getrlimit,_stubnosys);

__weak_alias(setrlimit,_stubnosys);
__weak_alias(_setrlimit,_stubnosys);
__strong_alias(_sys_setrlimit,_stubnosys);

__weak_alias(lseek,_stubnosys);
__weak_alias(_lseek,_stubnosys);
__strong_alias(_sys_lseek,_stubnosys);

__weak_alias(truncate,_stubnosys);
__weak_alias(_truncate,_stubnosys);
__strong_alias(_sys_truncate,_stubnosys);

__weak_alias(ftruncate,_stubnosys);
__weak_alias(_ftruncate,_stubnosys);
__strong_alias(_sys_ftruncate,_stubnosys);

__weak_alias(__sysctl,_stubnosys);
__weak_alias(___sysctl,_stubnosys);
__strong_alias(_sys___sysctl,_stubnosys);

__weak_alias(futimes,_stubnosys);
__weak_alias(_futimes,_stubnosys);
__strong_alias(_sys_futimes,_stubnosys);

__weak_alias(getpgid,_stubnosys);
__weak_alias(_getpgid,_stubnosys);
__strong_alias(_sys_getpgid,_stubnosys);

__weak_alias(reboot,_stubnosys);
__weak_alias(_reboot,_stubnosys);
__strong_alias(_sys_reboot,_stubnosys);

__weak_alias(poll,_stubnosys);
__weak_alias(_poll,_stubnosys);
__strong_alias(_sys_poll,_stubnosys);

__weak_alias(timer_create,_stubnosys);
__weak_alias(_timer_create,_stubnosys);
__strong_alias(_sys_timer_create,_stubnosys);

__weak_alias(timer_delete,_stubnosys);
__weak_alias(_timer_delete,_stubnosys);
__strong_alias(_sys_timer_delete,_stubnosys);

__weak_alias(timer_getoverrun,_stubnosys);
__weak_alias(_timer_getoverrun,_stubnosys);
__strong_alias(_sys_timer_getoverrun,_stubnosys);

__weak_alias(fdatasync,_stubnosys);
__weak_alias(_fdatasync,_stubnosys);
__strong_alias(_sys_fdatasync,_stubnosys);

__weak_alias(modctl,_stubnosys);
__weak_alias(_modctl,_stubnosys);
__strong_alias(_sys_modctl,_stubnosys);

__weak_alias(_ksem_init,_stubnosys);
__weak_alias(__ksem_init,_stubnosys);
__strong_alias(_sys__ksem_init,_stubnosys);

__weak_alias(_ksem_open,_stubnosys);
__weak_alias(__ksem_open,_stubnosys);
__strong_alias(_sys__ksem_open,_stubnosys);

__weak_alias(_ksem_unlink,_stubnosys);
__weak_alias(__ksem_unlink,_stubnosys);
__strong_alias(_sys__ksem_unlink,_stubnosys);

__weak_alias(_ksem_close,_stubnosys);
__weak_alias(__ksem_close,_stubnosys);
__strong_alias(_sys__ksem_close,_stubnosys);

__weak_alias(_ksem_post,_stubnosys);
__weak_alias(__ksem_post,_stubnosys);
__strong_alias(_sys__ksem_post,_stubnosys);

__weak_alias(_ksem_wait,_stubnosys);
__weak_alias(__ksem_wait,_stubnosys);
__strong_alias(_sys__ksem_wait,_stubnosys);

__weak_alias(_ksem_trywait,_stubnosys);
__weak_alias(__ksem_trywait,_stubnosys);
__strong_alias(_sys__ksem_trywait,_stubnosys);

__weak_alias(_ksem_getvalue,_stubnosys);
__weak_alias(__ksem_getvalue,_stubnosys);
__strong_alias(_sys__ksem_getvalue,_stubnosys);

__weak_alias(_ksem_destroy,_stubnosys);
__weak_alias(__ksem_destroy,_stubnosys);
__strong_alias(_sys__ksem_destroy,_stubnosys);

__weak_alias(_ksem_timedwait,_stubnosys);
__weak_alias(__ksem_timedwait,_stubnosys);
__strong_alias(_sys__ksem_timedwait,_stubnosys);

__weak_alias(__posix_rename,_stubnosys);
__weak_alias(___posix_rename,_stubnosys);
__strong_alias(_sys___posix_rename,_stubnosys);

__weak_alias(lchmod,_stubnosys);
__weak_alias(_lchmod,_stubnosys);
__strong_alias(_sys_lchmod,_stubnosys);

__weak_alias(lchown,_stubnosys);
__weak_alias(_lchown,_stubnosys);
__strong_alias(_sys_lchown,_stubnosys);

__weak_alias(lutimes,_stubnosys);
__weak_alias(_lutimes,_stubnosys);
__strong_alias(_sys_lutimes,_stubnosys);

__weak_alias(getsid,_stubnosys);
__weak_alias(_getsid,_stubnosys);
__strong_alias(_sys_getsid,_stubnosys);

__weak_alias(fktrace,_stubnosys);
__weak_alias(_fktrace,_stubnosys);
__strong_alias(_sys_fktrace,_stubnosys);

__weak_alias(preadv,_stubnosys);
__weak_alias(_preadv,_stubnosys);
__strong_alias(_sys_preadv,_stubnosys);

__weak_alias(pwritev,_stubnosys);
__weak_alias(_pwritev,_stubnosys);
__strong_alias(_sys_pwritev,_stubnosys);

__weak_alias(__getcwd,_stubnosys);
__weak_alias(___getcwd,_stubnosys);
__strong_alias(_sys___getcwd,_stubnosys);

__weak_alias(fchroot,_stubnosys);
__weak_alias(_fchroot,_stubnosys);
__strong_alias(_sys_fchroot,_stubnosys);

__weak_alias(lchflags,_stubnosys);
__weak_alias(_lchflags,_stubnosys);
__strong_alias(_sys_lchflags,_stubnosys);

__weak_alias(issetugid,_stubnosys);
__weak_alias(_issetugid,_stubnosys);
__strong_alias(_sys_issetugid,_stubnosys);

__weak_alias(utrace,_stubnosys);
__weak_alias(_utrace,_stubnosys);
__strong_alias(_sys_utrace,_stubnosys);

__weak_alias(kqueue,_stubnosys);
__weak_alias(_kqueue,_stubnosys);
__strong_alias(_sys_kqueue,_stubnosys);

__weak_alias(kevent,_stubnosys);
__weak_alias(_kevent,_stubnosys);
__strong_alias(_sys_kevent,_stubnosys);

__weak_alias(fsync_range,_stubnosys);
__weak_alias(_fsync_range,_stubnosys);
__strong_alias(_sys_fsync_range,_stubnosys);

__weak_alias(getvfsstat,_stubnosys);
__weak_alias(_getvfsstat,_stubnosys);
__strong_alias(_sys_getvfsstat,_stubnosys);

__weak_alias(statvfs1,_stubnosys);
__weak_alias(_statvfs1,_stubnosys);
__strong_alias(_sys_statvfs1,_stubnosys);

__weak_alias(fstatvfs1,_stubnosys);
__weak_alias(_fstatvfs1,_stubnosys);
__strong_alias(_sys_fstatvfs1,_stubnosys);

__weak_alias(extattrctl,_stubnosys);
__weak_alias(_extattrctl,_stubnosys);
__strong_alias(_sys_extattrctl,_stubnosys);

__weak_alias(extattr_set_file,_stubnosys);
__weak_alias(_extattr_set_file,_stubnosys);
__strong_alias(_sys_extattr_set_file,_stubnosys);

__weak_alias(extattr_get_file,_stubnosys);
__weak_alias(_extattr_get_file,_stubnosys);
__strong_alias(_sys_extattr_get_file,_stubnosys);

__weak_alias(extattr_delete_file,_stubnosys);
__weak_alias(_extattr_delete_file,_stubnosys);
__strong_alias(_sys_extattr_delete_file,_stubnosys);

__weak_alias(extattr_set_fd,_stubnosys);
__weak_alias(_extattr_set_fd,_stubnosys);
__strong_alias(_sys_extattr_set_fd,_stubnosys);

__weak_alias(extattr_get_fd,_stubnosys);
__weak_alias(_extattr_get_fd,_stubnosys);
__strong_alias(_sys_extattr_get_fd,_stubnosys);

__weak_alias(extattr_delete_fd,_stubnosys);
__weak_alias(_extattr_delete_fd,_stubnosys);
__strong_alias(_sys_extattr_delete_fd,_stubnosys);

__weak_alias(extattr_set_link,_stubnosys);
__weak_alias(_extattr_set_link,_stubnosys);
__strong_alias(_sys_extattr_set_link,_stubnosys);

__weak_alias(extattr_get_link,_stubnosys);
__weak_alias(_extattr_get_link,_stubnosys);
__strong_alias(_sys_extattr_get_link,_stubnosys);

__weak_alias(extattr_delete_link,_stubnosys);
__weak_alias(_extattr_delete_link,_stubnosys);
__strong_alias(_sys_extattr_delete_link,_stubnosys);

__weak_alias(extattr_list_fd,_stubnosys);
__weak_alias(_extattr_list_fd,_stubnosys);
__strong_alias(_sys_extattr_list_fd,_stubnosys);

__weak_alias(extattr_list_file,_stubnosys);
__weak_alias(_extattr_list_file,_stubnosys);
__strong_alias(_sys_extattr_list_file,_stubnosys);

__weak_alias(extattr_list_link,_stubnosys);
__weak_alias(_extattr_list_link,_stubnosys);
__strong_alias(_sys_extattr_list_link,_stubnosys);

__weak_alias(pselect,_stubnosys);
__weak_alias(_pselect,_stubnosys);
__strong_alias(_sys_pselect,_stubnosys);

__weak_alias(pollts,_stubnosys);
__weak_alias(_pollts,_stubnosys);
__strong_alias(_sys_pollts,_stubnosys);

__weak_alias(setxattr,_stubnosys);
__weak_alias(_setxattr,_stubnosys);
__strong_alias(_sys_setxattr,_stubnosys);

__weak_alias(lsetxattr,_stubnosys);
__weak_alias(_lsetxattr,_stubnosys);
__strong_alias(_sys_lsetxattr,_stubnosys);

__weak_alias(fsetxattr,_stubnosys);
__weak_alias(_fsetxattr,_stubnosys);
__strong_alias(_sys_fsetxattr,_stubnosys);

__weak_alias(getxattr,_stubnosys);
__weak_alias(_getxattr,_stubnosys);
__strong_alias(_sys_getxattr,_stubnosys);

__weak_alias(lgetxattr,_stubnosys);
__weak_alias(_lgetxattr,_stubnosys);
__strong_alias(_sys_lgetxattr,_stubnosys);

__weak_alias(fgetxattr,_stubnosys);
__weak_alias(_fgetxattr,_stubnosys);
__strong_alias(_sys_fgetxattr,_stubnosys);

__weak_alias(listxattr,_stubnosys);
__weak_alias(_listxattr,_stubnosys);
__strong_alias(_sys_listxattr,_stubnosys);

__weak_alias(llistxattr,_stubnosys);
__weak_alias(_llistxattr,_stubnosys);
__strong_alias(_sys_llistxattr,_stubnosys);

__weak_alias(flistxattr,_stubnosys);
__weak_alias(_flistxattr,_stubnosys);
__strong_alias(_sys_flistxattr,_stubnosys);

__weak_alias(removexattr,_stubnosys);
__weak_alias(_removexattr,_stubnosys);
__strong_alias(_sys_removexattr,_stubnosys);

__weak_alias(lremovexattr,_stubnosys);
__weak_alias(_lremovexattr,_stubnosys);
__strong_alias(_sys_lremovexattr,_stubnosys);

__weak_alias(fremovexattr,_stubnosys);
__weak_alias(_fremovexattr,_stubnosys);
__strong_alias(_sys_fremovexattr,_stubnosys);

__weak_alias(__stat30,_stubnosys);
__weak_alias(___stat30,_stubnosys);
__strong_alias(_sys___stat30,_stubnosys);

__weak_alias(__fstat30,_stubnosys);
__weak_alias(___fstat30,_stubnosys);
__strong_alias(_sys___fstat30,_stubnosys);

__weak_alias(__lstat30,_stubnosys);
__weak_alias(___lstat30,_stubnosys);
__strong_alias(_sys___lstat30,_stubnosys);

__weak_alias(__getdents30,_stubnosys);
__weak_alias(___getdents30,_stubnosys);
__strong_alias(_sys___getdents30,_stubnosys);

__weak_alias(__socket30,_stubnosys);
__weak_alias(___socket30,_stubnosys);
__strong_alias(_sys___socket30,_stubnosys);

__weak_alias(__getfh30,_stubnosys);
__weak_alias(___getfh30,_stubnosys);
__strong_alias(_sys___getfh30,_stubnosys);

__weak_alias(__fhopen40,_stubnosys);
__weak_alias(___fhopen40,_stubnosys);
__strong_alias(_sys___fhopen40,_stubnosys);

__weak_alias(__fhstatvfs140,_stubnosys);
__weak_alias(___fhstatvfs140,_stubnosys);
__strong_alias(_sys___fhstatvfs140,_stubnosys);

__weak_alias(__fhstat40,_stubnosys);
__weak_alias(___fhstat40,_stubnosys);
__strong_alias(_sys___fhstat40,_stubnosys);

__weak_alias(aio_cancel,_stubnosys);
__weak_alias(_aio_cancel,_stubnosys);
__strong_alias(_sys_aio_cancel,_stubnosys);

__weak_alias(aio_error,_stubnosys);
__weak_alias(_aio_error,_stubnosys);
__strong_alias(_sys_aio_error,_stubnosys);

__weak_alias(aio_fsync,_stubnosys);
__weak_alias(_aio_fsync,_stubnosys);
__strong_alias(_sys_aio_fsync,_stubnosys);

__weak_alias(aio_read,_stubnosys);
__weak_alias(_aio_read,_stubnosys);
__strong_alias(_sys_aio_read,_stubnosys);

__weak_alias(aio_return,_stubnosys);
__weak_alias(_aio_return,_stubnosys);
__strong_alias(_sys_aio_return,_stubnosys);

__weak_alias(aio_write,_stubnosys);
__weak_alias(_aio_write,_stubnosys);
__strong_alias(_sys_aio_write,_stubnosys);

__weak_alias(lio_listio,_stubnosys);
__weak_alias(_lio_listio,_stubnosys);
__strong_alias(_sys_lio_listio,_stubnosys);

__weak_alias(__mount50,_stubnosys);
__weak_alias(___mount50,_stubnosys);
__strong_alias(_sys___mount50,_stubnosys);

__weak_alias(__posix_fadvise50,_stubnosys);
__weak_alias(___posix_fadvise50,_stubnosys);
__strong_alias(_sys___posix_fadvise50,_stubnosys);

__weak_alias(__select50,_stubnosys);
__weak_alias(___select50,_stubnosys);
__strong_alias(_sys___select50,_stubnosys);

__weak_alias(__gettimeofday50,_stubnosys);
__weak_alias(___gettimeofday50,_stubnosys);
__strong_alias(_sys___gettimeofday50,_stubnosys);

__weak_alias(__settimeofday50,_stubnosys);
__weak_alias(___settimeofday50,_stubnosys);
__strong_alias(_sys___settimeofday50,_stubnosys);

__weak_alias(__utimes50,_stubnosys);
__weak_alias(___utimes50,_stubnosys);
__strong_alias(_sys___utimes50,_stubnosys);

__weak_alias(__adjtime50,_stubnosys);
__weak_alias(___adjtime50,_stubnosys);
__strong_alias(_sys___adjtime50,_stubnosys);

__weak_alias(__futimes50,_stubnosys);
__weak_alias(___futimes50,_stubnosys);
__strong_alias(_sys___futimes50,_stubnosys);

__weak_alias(__lutimes50,_stubnosys);
__weak_alias(___lutimes50,_stubnosys);
__strong_alias(_sys___lutimes50,_stubnosys);

__weak_alias(__setitimer50,_stubnosys);
__weak_alias(___setitimer50,_stubnosys);
__strong_alias(_sys___setitimer50,_stubnosys);

__weak_alias(__getitimer50,_stubnosys);
__weak_alias(___getitimer50,_stubnosys);
__strong_alias(_sys___getitimer50,_stubnosys);

__weak_alias(__clock_gettime50,_stubnosys);
__weak_alias(___clock_gettime50,_stubnosys);
__strong_alias(_sys___clock_gettime50,_stubnosys);

__weak_alias(__clock_settime50,_stubnosys);
__weak_alias(___clock_settime50,_stubnosys);
__strong_alias(_sys___clock_settime50,_stubnosys);

__weak_alias(__clock_getres50,_stubnosys);
__weak_alias(___clock_getres50,_stubnosys);
__strong_alias(_sys___clock_getres50,_stubnosys);

__weak_alias(__nanosleep50,_stubnosys);
__weak_alias(___nanosleep50,_stubnosys);
__strong_alias(_sys___nanosleep50,_stubnosys);

__weak_alias(__kevent50,_stubnosys);
__weak_alias(___kevent50,_stubnosys);
__strong_alias(_sys___kevent50,_stubnosys);

__weak_alias(__pselect50,_stubnosys);
__weak_alias(___pselect50,_stubnosys);
__strong_alias(_sys___pselect50,_stubnosys);

__weak_alias(__pollts50,_stubnosys);
__weak_alias(___pollts50,_stubnosys);
__strong_alias(_sys___pollts50,_stubnosys);

__weak_alias(__aio_suspend50,_stubnosys);
__weak_alias(___aio_suspend50,_stubnosys);
__strong_alias(_sys___aio_suspend50,_stubnosys);

__weak_alias(__stat50,_stubnosys);
__weak_alias(___stat50,_stubnosys);
__strong_alias(_sys___stat50,_stubnosys);

__weak_alias(__fstat50,_stubnosys);
__weak_alias(___fstat50,_stubnosys);
__strong_alias(_sys___fstat50,_stubnosys);

__weak_alias(__lstat50,_stubnosys);
__weak_alias(___lstat50,_stubnosys);
__strong_alias(_sys___lstat50,_stubnosys);

__weak_alias(__timer_settime50,_stubnosys);
__weak_alias(___timer_settime50,_stubnosys);
__strong_alias(_sys___timer_settime50,_stubnosys);

__weak_alias(__timer_gettime50,_stubnosys);
__weak_alias(___timer_gettime50,_stubnosys);
__strong_alias(_sys___timer_gettime50,_stubnosys);

__weak_alias(__mknod50,_stubnosys);
__weak_alias(___mknod50,_stubnosys);
__strong_alias(_sys___mknod50,_stubnosys);

__weak_alias(__fhstat50,_stubnosys);
__weak_alias(___fhstat50,_stubnosys);
__strong_alias(_sys___fhstat50,_stubnosys);

__weak_alias(pipe2,_stubnosys);
__weak_alias(_pipe2,_stubnosys);
__strong_alias(_sys_pipe2,_stubnosys);

__weak_alias(dup3,_stubnosys);
__weak_alias(_dup3,_stubnosys);
__strong_alias(_sys_dup3,_stubnosys);

__weak_alias(kqueue1,_stubnosys);
__weak_alias(_kqueue1,_stubnosys);
__strong_alias(_sys_kqueue1,_stubnosys);

__weak_alias(paccept,_stubnosys);
__weak_alias(_paccept,_stubnosys);
__strong_alias(_sys_paccept,_stubnosys);

__weak_alias(linkat,_stubnosys);
__weak_alias(_linkat,_stubnosys);
__strong_alias(_sys_linkat,_stubnosys);

__weak_alias(renameat,_stubnosys);
__weak_alias(_renameat,_stubnosys);
__strong_alias(_sys_renameat,_stubnosys);

__weak_alias(mkfifoat,_stubnosys);
__weak_alias(_mkfifoat,_stubnosys);
__strong_alias(_sys_mkfifoat,_stubnosys);

__weak_alias(mknodat,_stubnosys);
__weak_alias(_mknodat,_stubnosys);
__strong_alias(_sys_mknodat,_stubnosys);

__weak_alias(mkdirat,_stubnosys);
__weak_alias(_mkdirat,_stubnosys);
__strong_alias(_sys_mkdirat,_stubnosys);

__weak_alias(faccessat,_stubnosys);
__weak_alias(_faccessat,_stubnosys);
__strong_alias(_sys_faccessat,_stubnosys);

__weak_alias(fchmodat,_stubnosys);
__weak_alias(_fchmodat,_stubnosys);
__strong_alias(_sys_fchmodat,_stubnosys);

__weak_alias(fchownat,_stubnosys);
__weak_alias(_fchownat,_stubnosys);
__strong_alias(_sys_fchownat,_stubnosys);

__weak_alias(fstatat,_stubnosys);
__weak_alias(_fstatat,_stubnosys);
__strong_alias(_sys_fstatat,_stubnosys);

__weak_alias(utimensat,_stubnosys);
__weak_alias(_utimensat,_stubnosys);
__strong_alias(_sys_utimensat,_stubnosys);

__weak_alias(openat,_stubnosys);
__weak_alias(_openat,_stubnosys);
__strong_alias(_sys_openat,_stubnosys);

__weak_alias(readlinkat,_stubnosys);
__weak_alias(_readlinkat,_stubnosys);
__strong_alias(_sys_readlinkat,_stubnosys);

__weak_alias(symlinkat,_stubnosys);
__weak_alias(_symlinkat,_stubnosys);
__strong_alias(_sys_symlinkat,_stubnosys);

__weak_alias(unlinkat,_stubnosys);
__weak_alias(_unlinkat,_stubnosys);
__strong_alias(_sys_unlinkat,_stubnosys);

__weak_alias(futimens,_stubnosys);
__weak_alias(_futimens,_stubnosys);
__strong_alias(_sys_futimens,_stubnosys);

__weak_alias(__quotactl,_stubnosys);
__weak_alias(___quotactl,_stubnosys);
__strong_alias(_sys___quotactl,_stubnosys);

__weak_alias(recvmmsg,_stubnosys);
__weak_alias(_recvmmsg,_stubnosys);
__strong_alias(_sys_recvmmsg,_stubnosys);

__weak_alias(sendmmsg,_stubnosys);
__weak_alias(_sendmmsg,_stubnosys);
__strong_alias(_sys_sendmmsg,_stubnosys);

__weak_alias(clock_nanosleep,_stubnosys);
__weak_alias(_clock_nanosleep,_stubnosys);
__strong_alias(_sys_clock_nanosleep,_stubnosys);

__weak_alias(posix_fallocate,_stubnosys);
__weak_alias(_posix_fallocate,_stubnosys);
__strong_alias(_sys_posix_fallocate,_stubnosys);

__weak_alias(fdiscard,_stubnosys);
__weak_alias(_fdiscard,_stubnosys);
__strong_alias(_sys_fdiscard,_stubnosys);
