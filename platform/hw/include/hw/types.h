#ifndef _BMK_TYPES_H_
#define _BMK_TYPES_H_

#include <bmk-core/types.h>
#include <hw/machine/types.h>

/*
 * MI types
 */

typedef unsigned long	u_long;
typedef unsigned int	u_int;
typedef unsigned short	u_short;
typedef unsigned char	u_char;

typedef uint32_t	mode_t;
typedef int64_t		off_t;
typedef uint64_t	dev_t;

typedef int		uid_t;
typedef int		gid_t;

#ifndef NULL
#define NULL (void *)0
#endif

#ifndef __dead
#define __dead
#endif

#ifndef __printflike
#define __printflike(a,b)
#endif

#endif /* _BMK_TYPES_H_ */
