#ifndef _BMK_COMMON_ERRNO_H_
#define _BMK_COMMON_ERRNO_H_

/*
 * errno values.
 * these "accidentally" match NetBSD ones for convenience reasons.
 *
 * NOTE: we don't slurp in the whole errno table for a reason.
 * Please be critical when adding new values!  Prefer to remove them!
 */

#define BMK_ENOENT		2
#define BMK_EIO			5
#define BMK_ENXIO		6
#define BMK_E2BIG		7
#define BMK_EBADF		9
#define BMK_ENOMEM		12
#define BMK_EBUSY		16
#define BMK_EINVAL		22
#define BMK_EROFS		30
#define BMK_ETIMEDOUT		60

#define BMK_EGENERIC		BMK_EINVAL

#endif /* _BMK_COMMON_ERRNO_H_ */
