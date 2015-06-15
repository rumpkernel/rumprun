/*
**  Copyright 1998-2003 University of Illinois Board of Trustees
**  Copyright 1998-2003 Mark D. Roth
**  All rights reserved.
**
**  libuntar.h - header file for libuntar library
**
**  Mark D. Roth <roth@uiuc.edu>
**  Campus Information Technologies and Educational Services
**  University of Illinois at Urbana-Champaign
*/

#ifndef LIBUNTAR_H
#define LIBUNTAR_H

#include <sys/types.h>
#include <sys/stat.h>
#include <tar.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef LIBUNTAR_STATIC
#define EXPORT static
#else
#define EXPORT
#endif

/* useful constants */
/* see FIXME note in block.c regarding T_BLOCKSIZE */
#define T_BLOCKSIZE		512
#define T_NAMELEN		100
#define T_PREFIXLEN		155
#define T_MAXPATHLEN		(T_NAMELEN + T_PREFIXLEN)

/* GNU extensions for typeflag */
#define GNU_LONGNAME_TYPE	'L'
#define GNU_LONGLINK_TYPE	'K'

/* our version of the tar header structure */
struct tar_header
{
	char name[100];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];
	char mtime[12];
	char chksum[8];
	char typeflag;
	char linkname[100];
	char magic[6];
	char version[2];
	char uname[32];
	char gname[32];
	char devmajor[8];
	char devminor[8];
	char prefix[155];
	char padding[12];
	char *gnu_longname;
	char *gnu_longlink;
};


/***** handle.c ************************************************************/

typedef int (*openfunc_t)(const char *, int, ...);
typedef int (*closefunc_t)(int);
typedef ssize_t (*readfunc_t)(int, void *, size_t);

typedef struct
{
	openfunc_t openfunc;
	closefunc_t closefunc;
	readfunc_t readfunc;
}
tartype_t;

typedef struct
{
	tartype_t *type;
	long fd;
	int oflags;
	int options;
	struct tar_header th_buf;
	char *th_pathname;
	int dirfd;
	int atflags;
}
TAR;

/* constant values for the TAR options field */
#define TAR_GNU			 1	/* use GNU extensions UNUSED */
#define TAR_VERBOSE		 2	/* output file info to stdout UNUSED */
#define TAR_NOOVERWRITE		 4	/* don't overwrite existing files */
#define TAR_IGNORE_EOT		 8	/* ignore double zero blocks as EOF */
#define TAR_CHECK_MAGIC		16	/* check magic in file header */
#define TAR_CHECK_VERSION	32	/* check version in file header */
#define TAR_IGNORE_CRC		64	/* ignore CRC in file header */
#define TAR_CHOWN	       128	/* chown files */

/* open a new tarfile handle */
EXPORT int
tar_open(TAR **t, const char *pathname, tartype_t *type,
	 int oflags, int mode, int options);

#if 0 /* UNUSED */
/* make a tarfile handle out of a previously-opened descriptor */
EXPORT int
tar_fdopen(TAR **t, int fd, const char *pathname, tartype_t *type,
	   int oflags, int mode, int options);

/* returns the descriptor associated with t */
EXPORT int
tar_fd(TAR *t);
#endif

/* close tarfile handle */
EXPORT int
tar_close(TAR *t);

/***** extract.c ***********************************************************/

/* extract groups of files */
EXPORT int
tar_extract_all(TAR *t, char *prefix);

#undef EXPORT

#ifdef __cplusplus
}
#endif

#endif /* ! LIBUNTAR_H */

