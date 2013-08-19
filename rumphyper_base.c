#include <mini-os/types.h>
#include <mini-os/console.h>

#include <xen/io/console.h>
#include <mini-os/xmalloc.h>
#include <mini-os/blkfront.h>
#include <mini-os/fcntl.h>

#include "rumphyper.h"

struct rumpuser_hyperup rumpuser__hyp;

int
rumpuser_init(int ver, const struct rumpuser_hyperup *hyp)
{

	rumpuser__hyp = *hyp;
	return 0;
}

void
rumpuser_putchar(int ch)
{

	printk("%c", ch);
}

static struct {
	const char *name;
	const char *value;
} envtab[] = {
	{ RUMPUSER_PARAM_NCPU, "1" },
	{ RUMPUSER_PARAM_HOSTNAME, "rump4xen" },
	{ "RUMP_VERBOSE", "1" },
	{ NULL, NULL },
};

int
rumpuser_getparam(const char *name, void *buf, size_t blen)
{
	int i;

	for (i = 0; envtab[i].name; i++) {
		if (strcmp(name, envtab[i].name) == 0) {
			if (blen < strlen(envtab[i].value)+1) {
				return RUMP_E2BIG;
			} else {
				strcpy(buf, envtab[i].value);
				return 0;
			}
		}
	}

        return RUMP_ENOENT;
}

/* Use same values both for absolute and relative clock. */
int
rumpuser_clock_gettime(int which, int64_t *sec, long *nsec)
{
	s_time_t time = NOW();

	*sec  = time / (1000*1000*1000ULL);
	*nsec = time % (1000*1000*1000ULL);

	return 0;
}

int
rumpuser_clock_sleep(int enum_rumpclock, int64_t sec, long nsec)
{
	enum rumpclock rclk = enum_rumpclock;
	struct thread *thread;
	uint32_t msec;
	int nlocks;

	rumpkern_unsched(&nlocks, NULL);
	switch (rclk) {
	case RUMPUSER_CLOCK_RELWALL:
		msec = sec * 1000 + nsec / (1000*1000UL);
		msleep(msec);
		break;
	case RUMPUSER_CLOCK_ABSMONO:
		thread = get_current();
		thread->wakeup_time = sec * (1000*1000*1000ULL) + nsec;
		clear_runnable(thread);
		schedule();
		break;
	}
	rumpkern_sched(nlocks, NULL);

	return 0;
}

int
rumpuser_malloc(size_t len, int alignment, void **retval)
{

	*retval = _xmalloc(len, alignment);
	return 0;
}

void
rumpuser_free(void *buf, size_t buflen)
{

	xfree(buf);
}

/* random until proven not.  eh eh eh */
int
rumpuser_getrandom(void *buf, size_t buflen, int flags, size_t *retp)
{
	static uint8_t rnd = 37;
	uint8_t *rndbuf;

	for (*retp = 0, rndbuf = buf; *retp < buflen; (*retp)++) {
		*rndbuf++ = rnd++;
	}

	return 0;
}

void
rumpuser_exit(int value)
{

	do_exit();
}

#define NBLKDEV 1
#define BLKFDOFF 375
static struct blkfront_dev *blkdevs[NBLKDEV];
static struct blkfront_info blkinfos[NBLKDEV];

/* TODO: refcount + close */
static int
devopen(int num)
{
	int nlocks;

	if (blkdevs[0])
		return 1;

	rumpkern_unsched(&nlocks, NULL);
	blkdevs[0] = init_blkfront(NULL, &blkinfos[0]);
	rumpkern_sched(nlocks, NULL);
	return blkdevs[0] != NULL;
}

int
rumpuser_open(const char *name, int mode, int *fdp)
{
	int acc;

	/* we support only the special case */
	if (strcmp(name, "blk0") != 0
	    || (mode & RUMPUSER_OPEN_BIO) == 0)
		return RUMP_EINVAL;

	if (!devopen(0))
		return RUMP_EPERM;

	acc = mode & RUMPUSER_OPEN_ACCMODE;
	if (acc == RUMPUSER_OPEN_WRONLY || acc == RUMPUSER_OPEN_RDWR) {
		if (blkinfos[0].mode != O_RDWR) {
			/* XXX: unopen */
			return RUMP_EROFS;
		}
	}

	*fdp = BLKFDOFF;
	return 0;
}

int
rumpuser_getfileinfo(const char *name, uint64_t *size, int *type)
{

	if (strcmp(name, "blk0") != 0)
		return RUMP_EXDEV;

	if (!devopen(0))
		return RUMP_EBADF;

	*size = blkinfos[0].sectors * blkinfos[0].sector_size;
	*type = RUMPUSER_FT_BLK;

	return 0;
}

struct biocb {
	struct blkfront_aiocb bio_aiocb;
	rump_biodone_fn bio_done;
	void *bio_arg;
	int complete;
};

static void
biocomp(struct blkfront_aiocb *aiocb, int ret)
{
	struct biocb *bio = aiocb->data;
	int nlocks;

	rumpkern_sched(0, NULL);
	if (ret)
		bio->bio_done(bio->bio_arg, 0, RUMP_EIO);
	else
		bio->bio_done(bio->bio_arg, bio->bio_aiocb.aio_nbytes, 0);
	rumpkern_unsched(&nlocks, NULL);
	bio->complete = 1;
}

void
rumpuser_bio(int fd, int op, void *data, size_t dlen, int64_t off,
	rump_biodone_fn biodone, void *donearg)
{
	struct biocb *bio = _xmalloc(sizeof(*bio), 8);
	struct blkfront_aiocb *aiocb = &bio->bio_aiocb;
	int nlocks;

	rumpkern_unsched(&nlocks, NULL);

	/* assert fd == 375 */

	bio->bio_done = biodone;
	bio->bio_arg = donearg;
	bio->complete = 0;

	aiocb->aio_dev = blkdevs[0];
	aiocb->aio_buf = data;
	aiocb->aio_nbytes = dlen;
	aiocb->aio_offset = off;
	aiocb->aio_cb = biocomp;
	aiocb->data  = bio;

	if (op & RUMPUSER_BIO_READ)
		blkfront_aio_read(aiocb);
	else
		blkfront_aio_write(aiocb);

	/* duh */
	while (!bio->complete) {
		blkfront_aio_poll(blkdevs[0]);
	}
	xfree(bio);

	rumpkern_sched(nlocks, NULL);
}

void
rumpuser_seterrno(int err)
{

	get_current()->threrrno = err;
}
