#ifndef _BMK_TYPES_H_
#define _BMK_TYPES_H_

#include <arch/i386/types.h>

/*
 * MI types
 */
typedef int64_t		bmk_time_t;

typedef unsigned long	u_long;
typedef unsigned int	u_int;
typedef unsigned short	u_short;
typedef unsigned char	u_char;

typedef uint32_t	mode_t;
typedef int64_t		off_t;
typedef uint64_t	dev_t;

typedef int		uid_t;
typedef int		gid_t;

#define NULL (void *)0
#define __dead
#define __printflike(a,b)

#endif /* _BMK_TYPES_H_ */
