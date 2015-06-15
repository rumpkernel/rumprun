/*
**  Copyright 1998-2003 University of Illinois Board of Trustees
**  Copyright 1998-2003 Mark D. Roth
**  All rights reserved.
**
**  Mark D. Roth <roth@uiuc.edu>
**  Campus Information Technologies and Educational Services
**  University of Illinois at Urbana-Champaign
*/

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <utime.h>
#include <libgen.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pwd.h>
#include <grp.h>

#include "libuntar.h"

#ifdef LIBUNTAR_STATIC
#define EXPORT static
#else
#define EXPORT
#endif

/* headers */

static int th_crc_calc(TAR *t);
static int th_signed_crc_calc(TAR *t);
/* compare checksums in a forgiving way */
#define th_crc_ok(t) (th_get_crc(t) == th_crc_calc(t) || th_get_crc(t) == th_signed_crc_calc(t))

static int oct_to_int(char *oct);

/* macros for reading/writing tarchive blocks */
#define tar_block_read(t, buf) \
        (*((t)->type->readfunc))((t)->fd, (char *)(buf), T_BLOCKSIZE)

/* read a header block */
static int th_read(TAR *t);

/* determine file type */
#define TH_ISREG(t)     ((t)->th_buf.typeflag == REGTYPE \
                         || (t)->th_buf.typeflag == AREGTYPE \
                         || (t)->th_buf.typeflag == CONTTYPE \
                         || (S_ISREG((mode_t)oct_to_int((t)->th_buf.mode)) \
                             && (t)->th_buf.typeflag != LNKTYPE))
#define TH_ISLNK(t)     ((t)->th_buf.typeflag == LNKTYPE)
#define TH_ISSYM(t)     ((t)->th_buf.typeflag == SYMTYPE \
                         || S_ISLNK((mode_t)oct_to_int((t)->th_buf.mode)))
#define TH_ISCHR(t)     ((t)->th_buf.typeflag == CHRTYPE \
                         || S_ISCHR((mode_t)oct_to_int((t)->th_buf.mode)))
#define TH_ISBLK(t)     ((t)->th_buf.typeflag == BLKTYPE \
                         || S_ISBLK((mode_t)oct_to_int((t)->th_buf.mode)))
#define TH_ISDIR(t)     ((t)->th_buf.typeflag == DIRTYPE \
                         || S_ISDIR((mode_t)oct_to_int((t)->th_buf.mode)) \
                         || ((t)->th_buf.typeflag == AREGTYPE \
                             && strlen((t)->th_buf.name) \
                             && ((t)->th_buf.name[strlen((t)->th_buf.name) - 1] == '/')))
#define TH_ISFIFO(t)    ((t)->th_buf.typeflag == FIFOTYPE \
                         || S_ISFIFO((mode_t)oct_to_int((t)->th_buf.mode)))
#define TH_ISLONGNAME(t)        ((t)->th_buf.typeflag == GNU_LONGNAME_TYPE)
#define TH_ISLONGLINK(t)        ((t)->th_buf.typeflag == GNU_LONGLINK_TYPE)

/* decode tar header info */
#define th_get_crc(t) oct_to_int((t)->th_buf.chksum)
/* We cast from int (what oct_to_int() returns) to
   unsigned int, to avoid unwieldy sign extensions
   from occurring on systems where size_t is bigger than int,
   since th_get_size() is often stored into a size_t. */
#define th_get_size(t) ((unsigned int)oct_to_int((t)->th_buf.size))
#define th_get_mtime(t) oct_to_int((t)->th_buf.mtime)
#define th_get_devmajor(t) oct_to_int((t)->th_buf.devmajor)
#define th_get_devminor(t) oct_to_int((t)->th_buf.devminor)
#define th_get_linkname(t) ((t)->th_buf.gnu_longlink \
                            ? (t)->th_buf.gnu_longlink \
                            : (t)->th_buf.linkname)
static char *th_get_pathname(TAR *t);
static mode_t th_get_mode(TAR *t);
static uid_t th_get_uid(TAR *t);
static gid_t th_get_gid(TAR *t);

/*
**  util.c - miscellaneous utility code for libtar
*/

/* calculate header checksum */
static int
th_crc_calc(TAR *t)
{
	int i, sum = 0;

	for (i = 0; i < T_BLOCKSIZE; i++)
		sum += ((unsigned char *)(&(t->th_buf)))[i];
	for (i = 0; i < 8; i++)
		sum += (' ' - (unsigned char)t->th_buf.chksum[i]);

	return sum;
}


/* calculate a signed header checksum */
static int
th_signed_crc_calc(TAR *t)
{
	int i, sum = 0;

	for (i = 0; i < T_BLOCKSIZE; i++)
		sum += ((signed char *)(&(t->th_buf)))[i];
	for (i = 0; i < 8; i++)
		sum += (' ' - (signed char)t->th_buf.chksum[i]);

	return sum;
}


/* string-octal to integer conversion */
static int
oct_to_int(char *oct)
{
	int i;

	return sscanf(oct, "%o", &i) == 1 ? i : 0;
}

/*
**  block.c - libtar code to handle tar archive header blocks
*/

#define BIT_ISSET(bitmask, bit) ((bitmask) & (bit))


/* read a header block */
/* FIXME: the return value of this function should match the return value
	  of tar_block_read(), which is a macro which references a prototype
	  that returns a ssize_t.  So far, this is safe, since tar_block_read()
	  only ever reads 512 (T_BLOCKSIZE) bytes at a time, so any difference
	  in size of ssize_t and int is of negligible risk.  BUT, if
	  T_BLOCKSIZE ever changes, or ever becomes a variable parameter
	  controllable by the user, all the code that calls it,
	  including this function and all code that calls it, should be
	  fixed for security reasons.
	  Thanks to Chris Palmer for the critique.
*/
static int
th_read_internal(TAR *t)
{
	int i;
	int num_zero_blocks = 0;

#ifdef DEBUG
	printf("==> th_read_internal\n");
#endif

	while ((i = tar_block_read(t, &(t->th_buf))) == T_BLOCKSIZE)
	{
		/* two all-zero blocks mark EOF */
		if (t->th_buf.name[0] == '\0')
		{
			num_zero_blocks++;
			if (!BIT_ISSET(t->options, TAR_IGNORE_EOT)
			    && num_zero_blocks >= 2)
				return 0;	/* EOF */
			else
				continue;
		}

		/* verify magic and version */
		if (BIT_ISSET(t->options, TAR_CHECK_MAGIC)
		    && strncmp(t->th_buf.magic, TMAGIC, TMAGLEN - 1) != 0)
		{
#ifdef DEBUG
			puts("!!! unknown magic value in tar header");
#endif
			return -2;
		}

		if (BIT_ISSET(t->options, TAR_CHECK_VERSION)
		    && strncmp(t->th_buf.version, TVERSION, TVERSLEN) != 0)
		{
#ifdef DEBUG
			puts("!!! unknown version value in tar header");
#endif
			return -2;
		}

		/* check chksum */
		if (!BIT_ISSET(t->options, TAR_IGNORE_CRC)
		    && !th_crc_ok(t))
		{
#ifdef DEBUG
			puts("!!! tar header checksum error");
#endif
			return -2;
		}

		break;
	}

#ifdef DEBUG
	printf("<== th_read_internal(): returning %d\n", i);
#endif
	return i;
}


/* wrapper function for th_read_internal() to handle GNU extensions */
static int
th_read(TAR *t)
{
	int i;
	size_t sz, j, blocks;
	char *ptr;

#ifdef DEBUG
	printf("==> th_read(t=0x%lx)\n", t);
#endif

	if (t->th_buf.gnu_longname != NULL)
		free(t->th_buf.gnu_longname);
	if (t->th_buf.gnu_longlink != NULL)
		free(t->th_buf.gnu_longlink);
	memset(&(t->th_buf), 0, sizeof(struct tar_header));

	i = th_read_internal(t);
	if (i == 0)
		return 1;
	else if (i != T_BLOCKSIZE)
	{
		if (i != -1)
			errno = EINVAL;
		return -1;
	}

	/* check for GNU long link extention */
	if (TH_ISLONGLINK(t))
	{
		sz = th_get_size(t);
		blocks = (sz / T_BLOCKSIZE) + (sz % T_BLOCKSIZE ? 1 : 0);
		if (blocks > ((size_t)-1 / T_BLOCKSIZE))
		{
			errno = E2BIG;
			return -1;
		}
#ifdef DEBUG
		printf("    th_read(): GNU long linkname detected "
		       "(%ld bytes, %d blocks)\n", sz, blocks);
#endif
		t->th_buf.gnu_longlink = (char *)malloc(blocks * T_BLOCKSIZE);
		if (t->th_buf.gnu_longlink == NULL)
			return -1;

		for (j = 0, ptr = t->th_buf.gnu_longlink; j < blocks;
		     j++, ptr += T_BLOCKSIZE)
		{
#ifdef DEBUG
			printf("    th_read(): reading long linkname "
			       "(%d blocks left, ptr == %ld)\n", blocks-j, ptr);
#endif
			i = tar_block_read(t, ptr);
			if (i != T_BLOCKSIZE)
			{
				if (i != -1)
					errno = EINVAL;
				return -1;
			}
#ifdef DEBUG
			printf("    th_read(): read block == \"%s\"\n", ptr);
#endif
		}
#ifdef DEBUG
		printf("    th_read(): t->th_buf.gnu_longlink == \"%s\"\n",
		       t->th_buf.gnu_longlink);
#endif

		i = th_read_internal(t);
		if (i != T_BLOCKSIZE)
		{
			if (i != -1)
				errno = EINVAL;
			return -1;
		}
	}

	/* check for GNU long name extention */
	if (TH_ISLONGNAME(t))
	{
		sz = th_get_size(t);
		blocks = (sz / T_BLOCKSIZE) + (sz % T_BLOCKSIZE ? 1 : 0);
		if (blocks > ((size_t)-1 / T_BLOCKSIZE))
		{
			errno = E2BIG;
			return -1;
		}
#ifdef DEBUG
		printf("    th_read(): GNU long filename detected "
		       "(%ld bytes, %d blocks)\n", sz, blocks);
#endif
		t->th_buf.gnu_longname = (char *)malloc(blocks * T_BLOCKSIZE);
		if (t->th_buf.gnu_longname == NULL)
			return -1;

		for (j = 0, ptr = t->th_buf.gnu_longname; j < blocks;
		     j++, ptr += T_BLOCKSIZE)
		{
#ifdef DEBUG
			printf("    th_read(): reading long filename "
			       "(%d blocks left, ptr == %ld)\n", blocks-j, ptr);
#endif
			i = tar_block_read(t, ptr);
			if (i != T_BLOCKSIZE)
			{
				if (i != -1)
					errno = EINVAL;
				return -1;
			}
#ifdef DEBUG
			printf("    th_read(): read block == \"%s\"\n", ptr);
#endif
		}
#ifdef DEBUG
		printf("    th_read(): t->th_buf.gnu_longname == \"%s\"\n",
		       t->th_buf.gnu_longname);
#endif

		i = th_read_internal(t);
		if (i != T_BLOCKSIZE)
		{
			if (i != -1)
				errno = EINVAL;
			return -1;
		}
	}

#if 0
	/*
	** work-around for old archive files with broken typeflag fields
	** NOTE: I fixed this in the TH_IS*() macros instead
	*/

	/*
	** (directories are signified with a trailing '/')
	*/
	if (t->th_buf.typeflag == AREGTYPE
	    && t->th_buf.name[strlen(t->th_buf.name) - 1] == '/')
		t->th_buf.typeflag = DIRTYPE;

	/*
	** fallback to using mode bits
	*/
	if (t->th_buf.typeflag == AREGTYPE)
	{
		mode = (mode_t)oct_to_int(t->th_buf.mode);

		if (S_ISREG(mode))
			t->th_buf.typeflag = REGTYPE;
		else if (S_ISDIR(mode))
			t->th_buf.typeflag = DIRTYPE;
		else if (S_ISFIFO(mode))
			t->th_buf.typeflag = FIFOTYPE;
		else if (S_ISCHR(mode))
			t->th_buf.typeflag = CHRTYPE;
		else if (S_ISBLK(mode))
			t->th_buf.typeflag = BLKTYPE;
		else if (S_ISLNK(mode))
			t->th_buf.typeflag = SYMTYPE;
	}
#endif

	return 0;
}

/*
**  Copyright 1998-2003 University of Illinois Board of Trustees
**  Copyright 1998-2003 Mark D. Roth
**  All rights reserved.
**
**  decode.c - libtar code to decode tar header blocks
**
**  Mark D. Roth <roth@uiuc.edu>
**  Campus Information Technologies and Educational Services
**  University of Illinois at Urbana-Champaign
*/

/* determine full path name */
static char *
th_get_pathname(TAR *t)
{
	if (t->th_buf.gnu_longname)
		return t->th_buf.gnu_longname;

	/* allocate the th_pathname buffer if not already */
	if (t->th_pathname == NULL)
	{
		t->th_pathname = malloc(MAXPATHLEN * sizeof(char));
		if (t->th_pathname == NULL)
			/* out of memory */
			return NULL;
	}

	if (t->th_buf.prefix[0] == '\0')
	{
		snprintf(t->th_pathname, MAXPATHLEN, "%.100s", t->th_buf.name);
	}
	else
	{
		snprintf(t->th_pathname, MAXPATHLEN, "%.155s/%.100s",
			 t->th_buf.prefix, t->th_buf.name);
	}

	/* will be deallocated in tar_close() */
	return t->th_pathname;
}


static uid_t
th_get_uid(TAR *t)
{
	int uid;
#ifdef USE_SYMBOLIC_IDS
	struct passwd *pw;

	pw = getpwnam(t->th_buf.uname);
	if (pw != NULL)
		return pw->pw_uid;

	/* if the password entry doesn't exist */
#endif
	sscanf(t->th_buf.uid, "%o", &uid);
	return uid;
}


static gid_t
th_get_gid(TAR *t)
{
	int gid;
#ifdef USE_SYMBOLIC_IDS
	struct group *gr;

	gr = getgrnam(t->th_buf.gname);
	if (gr != NULL)
		return gr->gr_gid;

	/* if the group entry doesn't exist */
#endif
	sscanf(t->th_buf.gid, "%o", &gid);
	return gid;
}

static mode_t
th_get_mode(TAR *t)
{
	mode_t mode;

	mode = (mode_t)oct_to_int(t->th_buf.mode);
	if (! (mode & S_IFMT))
	{
		switch (t->th_buf.typeflag)
		{
		case SYMTYPE:
			mode |= S_IFLNK;
			break;
		case CHRTYPE:
			mode |= S_IFCHR;
			break;
		case BLKTYPE:
			mode |= S_IFBLK;
			break;
		case DIRTYPE:
			mode |= S_IFDIR;
			break;
		case FIFOTYPE:
			mode |= S_IFIFO;
			break;
		case AREGTYPE:
			if (t->th_buf.name[strlen(t->th_buf.name) - 1] == '/')
			{
				mode |= S_IFDIR;
				break;
			}
			/* FALLTHROUGH */
		case LNKTYPE:
		case REGTYPE:
		default:
			mode |= S_IFREG;
		}
	}

	return mode;
}


/*
**  extract.c - libtar code to extract a file from a tar archive
*/

static int tar_extract_file(TAR *t, char *realname);

static int tar_extract_dir(TAR *t, char *filename);
static int tar_extract_hardlink(TAR *t, char *filename);
static int tar_extract_symlink(TAR *t, char *filename);
static int tar_extract_chardev(TAR *t, char *filename);
static int tar_extract_blockdev(TAR *t, char *filename);
static int tar_extract_fifo(TAR *t, char *filename);
static int tar_extract_regfile(TAR *t, char *filename);

EXPORT int
tar_extract_all(TAR *t, char *prefix)
{
	char *filename;
	int i;

#ifdef DEBUG
	printf("==> tar_extract_all(TAR *t, \"%s\")\n",
	       (prefix ? prefix : "(null)"));
#endif

	if (prefix) {
		t->dirfd = open(prefix, O_RDONLY | O_DIRECTORY);
		if (t->dirfd == -1)
			return -1;
	}

	while ((i = th_read(t)) == 0)
	{
#ifdef DEBUG
		puts("    tar_extract_all(): calling th_get_pathname()");
#endif
		filename = th_get_pathname(t);
#ifdef DEBUG
		printf("    tar_extract_all(): calling tar_extract_file(t, "
		       "\"%s\")\n", filename);
#endif
		if (tar_extract_file(t, filename) != 0)
			return -1;
	}

	if (t->dirfd >= 0) {
		close(t->dirfd);
	}

	return (i == 1 ? 0 : -1);
}

/*
** mkdirhier() - create all directories needed for a given filename
** returns:
**	0			success
**	1			all directories already exist
**	-1 (and sets errno)	error
*/
static int
mkdirhier(TAR *t, char *filename)
{
	char tmp[MAXPATHLEN], src[MAXPATHLEN], dst[MAXPATHLEN] = "";
	char *dirp, *nextp = src;
	int retval = 1;
	char *path;

	/* GNU dirname may modify string, so use temp buffer, sigh */
	
	if (strlen(filename) + 1 > sizeof(tmp))
	{
		errno = ENAMETOOLONG;
		return -1;
	}
	strncpy(tmp, filename, sizeof(tmp));

	path = dirname(tmp);
	/* path not longer than filename */
	strncpy(src, path, sizeof(src));

	if (path[0] == '/')
		strcpy(dst, "/");

	while ((dirp = strsep(&nextp, "/")) != NULL)
	{
		if (*dirp == '\0')
			continue;

		if (dst[0] != '\0')
			strcat(dst, "/");
		strcat(dst, dirp);

		if (mkdirat(t->dirfd, dst, 0777) == -1)
		{
			if (errno != EEXIST)
				return -1;
		}
		else
			retval = 0;
	}

	return retval;
}

static int
tar_set_file_perms(TAR *t, char *filename)
{
	mode_t mode = th_get_mode(t);
	uid_t uid = th_get_uid(t);
	gid_t gid = th_get_gid(t);
	time_t mtime = th_get_mtime(t);
	const struct timespec ut[] = {{mtime, 0}, {mtime, 0}};

	/* change owner/group */
	if (t->options & TAR_CHOWN)
		if (fchownat(t->dirfd, filename, uid, gid, t->atflags) == -1)
		{
#ifdef DEBUG
			fprintf(stderr, "fchownat(\"%s\", %d, %d): %s\n",
				filename, uid, gid, strerror(errno));
#endif
			return -1;
		}

	/* change access/modification time */
	if (!TH_ISSYM(t))
		if (utimensat(t->dirfd, filename, ut, t->atflags) == -1)
		{
#ifdef DEBUG
			perror("utimensat()");
#endif
			return -1;
		}

	/* change permissions */
	if (!TH_ISSYM(t))
		if (fchmodat(t->dirfd, filename, mode, 0) == -1)
	{
#ifdef DEBUG
		perror("fchmodat()");
#endif
		return -1;
	}

	return 0;
}


/* switchboard */
static int
tar_extract_file(TAR *t, char *realname)
{
	int i;

	if (t->options & TAR_NOOVERWRITE)
	{
		struct stat s;

		if (fstatat(t->dirfd, realname, &s, t->atflags) == 0 || errno != ENOENT)
		{
			errno = EEXIST;
			return -1;
		}
	}

	if (TH_ISDIR(t))
		i = tar_extract_dir(t, realname);
	else if (TH_ISLNK(t))
		i = tar_extract_hardlink(t, realname);
	else if (TH_ISSYM(t))
		i = tar_extract_symlink(t, realname);
	else if (TH_ISCHR(t))
		i = tar_extract_chardev(t, realname);
	else if (TH_ISBLK(t))
		i = tar_extract_blockdev(t, realname);
	else if (TH_ISFIFO(t))
		i = tar_extract_fifo(t, realname);
	else if (TH_ISREG(t))
		i = tar_extract_regfile(t, realname);
	else
		return -1;

	if (i != 0)
		return i;

	i = tar_set_file_perms(t, realname);
	if (i != 0)
		return i;

	return 0;
}


/* extract regular file */
static int
tar_extract_regfile(TAR *t, char *filename)
{
	size_t size = th_get_size(t);
	int fdout;
	int i, k;
	char buf[T_BLOCKSIZE];

#ifdef DEBUG
	printf("==> tar_extract_regfile(t=0x%lx, realname=\"%s\")\n", t,
	       filename);
#endif

	if (mkdirhier(t, filename) == -1)
		return -1;

#ifdef DEBUG
	printf("  ==> extracting: %s (mode %04o, uid %d, gid %d, %d bytes)\n",
	       filename, mode, uid, gid, size);
#endif
	fdout = openat(t->dirfd, filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fdout == -1)
	{
#ifdef DEBUG
		perror("openat()");
#endif
		return -1;
	}

	/* extract the file */
	for (i = size; i > 0; i -= T_BLOCKSIZE)
	{
		k = tar_block_read(t, buf);
		if (k != T_BLOCKSIZE)
		{
			if (k != -1)
				errno = EINVAL;
			close(fdout);
			return -1;
		}

		/* write block to output file */
		if (write(fdout, buf,
			  ((i > T_BLOCKSIZE) ? T_BLOCKSIZE : i)) == -1)
		{
			close(fdout);
			return -1;
		}
	}

	/* close output file */
	if (close(fdout) == -1)
		return -1;

#ifdef DEBUG
	printf("### done extracting %s\n", filename);
#endif

	return 0;
}


/* hardlink */
static int
tar_extract_hardlink(TAR * t, char *filename)
{
	char *linktgt = th_get_linkname(t);

	if (mkdirhier(t, filename) == -1)
		return -1;

#ifdef DEBUG
	printf("  ==> extracting: %s (link to %s)\n", filename, linktgt);
#endif
	if (linkat(t->dirfd, linktgt, t->dirfd, filename, t->atflags) == -1)
	{
#ifdef DEBUG
		perror("linkat()");
#endif
		return -1;
	}

	return 0;
}


/* symlink */
static int
tar_extract_symlink(TAR *t, char *filename)
{

	if (mkdirhier(t, filename) == -1)
		return -1;

	if (unlink(filename) == -1 && errno != ENOENT)
		return -1;

#ifdef DEBUG
	printf("  ==> extracting: %s (symlink to %s)\n",
	       filename, th_get_linkname(t));
#endif
	if (symlinkat(th_get_linkname(t), t->dirfd, filename) == -1)
	{
#ifdef DEBUG
		perror("symlinkat()");
#endif
		return -1;
	}

	return 0;
}


/* character device */
static int
tar_extract_chardev(TAR *t, char *filename)
{
	mode_t mode = th_get_mode(t);
	unsigned long devmaj = th_get_devmajor(t);
	unsigned long devmin = th_get_devminor(t);

	if (mkdirhier(t, filename) == -1)
		return -1;

#ifdef DEBUG
	printf("  ==> extracting: %s (character device %ld,%ld)\n",
	       filename, devmaj, devmin);
#endif
	if (mknodat(t->dirfd, filename, mode | S_IFCHR,
		  makedev(devmaj, devmin)) == -1)
	{
#ifdef DEBUG
		perror("mknodat()");
#endif
		return -1;
	}

	return 0;
}


/* block device */
static int
tar_extract_blockdev(TAR *t, char *filename)
{
	mode_t mode = th_get_mode(t);
	unsigned long devmaj = th_get_devmajor(t);
	unsigned long devmin = th_get_devminor(t);

	if (mkdirhier(t, filename) == -1)
		return -1;

#ifdef DEBUG
	printf("  ==> extracting: %s (block device %ld,%ld)\n",
	       filename, devmaj, devmin);
#endif
	if (mknodat(t->dirfd, filename, mode | S_IFBLK,
		  makedev(devmaj, devmin)) == -1)
	{
#ifdef DEBUG
		perror("mknodat()");
#endif
		return -1;
	}

	return 0;
}


/* directory */
static int
tar_extract_dir(TAR *t, char *filename)
{
	mode_t mode;

	mode = th_get_mode(t);

	if (mkdirhier(t, filename) == -1)
		return -1;

#ifdef DEBUG
	printf("  ==> extracting: %s (mode %04o, directory)\n", filename,
	       mode);
#endif
	if (mkdirat(t->dirfd, filename, mode) == -1)
	{
		if (errno == EEXIST)
		{
			if (fchmodat(t->dirfd, filename, mode, 0) == -1)
			{
#ifdef DEBUG
				perror("fchmodat()");
#endif
				return -1;
			}
			else
			{
#ifdef DEBUG
				puts("  *** using existing directory");
#endif
				return 0;
			}
		}
		else
		{
#ifdef DEBUG
			perror("mkdirat()");
#endif
			return -1;
		}
	}

	return 0;
}


/* FIFO */
static int
tar_extract_fifo(TAR *t, char *filename)
{
	mode_t mode;

	mode = th_get_mode(t);

	if (mkdirhier(t, filename) == -1)
		return -1;

#ifdef DEBUG
	printf("  ==> extracting: %s (fifo)\n", filename);
#endif
	if (mkfifoat(t->dirfd, filename, mode) == -1)
	{
#ifdef DEBUG
		perror("mkfifoat()");
#endif
		return -1;
	}

	return 0;
}

/*
**  handle.c - libtar code for initializing a TAR handle
*/

static tartype_t default_type = { open, close, read };

static int
tar_init(TAR **t, tartype_t *type,
	 int oflags, int mode, int options)
{
	if ((oflags & O_ACCMODE) == O_RDWR)
	{
		errno = EINVAL;
		return -1;
	}

	*t = (TAR *)calloc(1, sizeof(TAR));
	if (*t == NULL)
		return -1;

	(*t)->options = options;
	(*t)->type = (type ? type : &default_type);
	(*t)->oflags = oflags;
	(*t)->dirfd = AT_FDCWD;
	(*t)->atflags = AT_SYMLINK_NOFOLLOW;

	return 0;
}

/* open a new tarfile handle */
EXPORT int
tar_open(TAR **t, const char *pathname, tartype_t *type,
	 int oflags, int mode, int options)
{
	if (tar_init(t, type, oflags, mode, options) == -1)
		return -1;

	if ((options & TAR_NOOVERWRITE) && (oflags & O_CREAT))
		oflags |= O_EXCL;

	(*t)->fd = (*((*t)->type->openfunc))(pathname, oflags, mode);
	if ((*t)->fd == -1)
	{
		free(*t);
		return -1;
	}

	return 0;
}


#if 0 /* UNUSED */
EXPORT int
tar_fdopen(TAR **t, int fd, const char *pathname, tartype_t *type,
	   int oflags, int mode, int options)
{
	if (tar_init(t, type, oflags, mode, options) == -1)
		return -1;

	(*t)->fd = fd;
	return 0;
}

EXPORT int
tar_fd(TAR *t)
{

	return t->fd;
}
#endif

/* close tarfile handle */
EXPORT int
tar_close(TAR *t)
{
	int i;

	i = (*(t->type->closefunc))(t->fd);

	if (t->th_pathname != NULL)
		free(t->th_pathname);
	free(t);

	return i;
}

#undef EXPORT
