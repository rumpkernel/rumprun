#ifndef _NOLIBC_H_
#define _NOLIBC_H_

#include <bmk-core/types.h>
#include <bmk-core/sched.h>

/* fake some POSIX types used in the compat system calls */
#define RUMP_HOST_NOT_POSIX

typedef int		clockid_t;
typedef unsigned int	socklen_t;
typedef int		timer_t;

typedef int		pid_t;
typedef long		ssize_t;
typedef unsigned long	size_t;
typedef long		register_t;

struct timespec;
struct itimerspec;
struct sigevent;
struct sockaddr;

typedef void fd_set;
typedef void sigset_t;

#endif /* _NOLIBC_H_ */
