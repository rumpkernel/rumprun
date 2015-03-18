#ifndef _BMK_COMMON_ERRNO_H_
#define _BMK_COMMON_ERRNO_H_

/*
 * errno values.
 * these "accidentally" match NetBSD ones for convenience reasons.
 */

#define BMK_ENOMEM              12
#define BMK_EBUSY               16
#define BMK_EINVAL              22
#define BMK_ETIMEDOUT           60

#define BMK_EGENERIC            BMK_EINVAL

#endif /* _BMK_COMMON_ERRNO_H_ */
