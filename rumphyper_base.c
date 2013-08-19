/*-
 * Copyright (c) 2013 Antti Kantee.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <mini-os/types.h>
#include <mini-os/console.h>

#include <xen/io/console.h>
#include <mini-os/xmalloc.h>
#include <mini-os/blkfront.h>
#include <mini-os/fcntl.h>

#include "rumphyper.h"

struct rumpuser_hyperup rumpuser__hyp;

static void biothread(void *arg);
static struct rumpuser_mtx *bio_mtx;
static struct rumpuser_cv *bio_cv;
static int bio_outstanding;

int
rumpuser_init(int ver, const struct rumpuser_hyperup *hyp)
{

	rumpuser__hyp = *hyp;

	rumpuser_mutex_init(&bio_mtx, RUMPUSER_MTX_SPIN);
	rumpuser_cv_init(&bio_cv);
	create_thread("biopoll", biothread, NULL);

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

/* Not very random */
int
rumpuser_getrandom(void *buf, size_t buflen, int flags, size_t *retp)
{
	uint8_t *rndbuf;

	for (*retp = 0, rndbuf = buf; *retp < buflen; (*retp)++) {
		*rndbuf++ = NOW() & 0xff;
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
static int blkopen[NBLKDEV];

static int
devopen(int num)
{
	int nlocks;

	if (num+1 > NBLKDEV)
		return RUMP_ENXIO;

	if (blkopen[num]) {
		blkopen[num]++;
		return 1;
	}

	rumpkern_unsched(&nlocks, NULL);
	blkdevs[num] = init_blkfront(NULL, &blkinfos[num]);
	rumpkern_sched(nlocks, NULL);

	if (blkdevs[num] != NULL) {
		blkopen[num] = 1;
		return 0;
	} else {
		return RUMP_EIO; /* guess something */
	}
}

int
rumpuser_open(const char *name, int mode, int *fdp)
{
	int acc, rv;

	/* we support only the special case */
	if (strcmp(name, "blk0") != 0
	    || (mode & RUMPUSER_OPEN_BIO) == 0)
		return RUMP_EINVAL;

	if ((rv = devopen(0)) != 0)
		return rv;

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
rumpuser_close(int fd)
{
	int rfd = fd - BLKFDOFF;

	if (rfd < 0 || rfd+1 > NBLKDEV)
		return RUMP_EBADF;

	if (--blkopen[rfd] == 0) {
		struct blkfront_dev *toclose = blkdevs[rfd];
		
		/* not sure if this appropriately prevents races either ... */
		blkdevs[rfd] = NULL;
		shutdown_blkfront(toclose);
		
	}

	return 0;
}

int
rumpuser_getfileinfo(const char *name, uint64_t *size, int *type)
{
	int rv;

	if (strcmp(name, "blk0") != 0)
		return RUMP_EXDEV;

	if ((rv = devopen(0)) != 0)
		return rv;

	*size = blkinfos[0].sectors * blkinfos[0].sector_size;
	*type = RUMPUSER_FT_BLK;

	rumpuser_close(BLKFDOFF);

	return 0;
}

struct biocb {
	struct blkfront_aiocb bio_aiocb;
	rump_biodone_fn bio_done;
	void *bio_arg;
};

static void
biocomp(struct blkfront_aiocb *aiocb, int ret)
{
	struct biocb *bio = aiocb->data;
	int dummy;

	rumpkern_sched(0, NULL);
	if (ret)
		bio->bio_done(bio->bio_arg, 0, RUMP_EIO);
	else
		bio->bio_done(bio->bio_arg, bio->bio_aiocb.aio_nbytes, 0);
	rumpkern_unsched(&dummy, NULL);
	xfree(bio);

	rumpuser_mutex_enter_nowrap(bio_mtx);
	bio_outstanding--;
	rumpuser_mutex_exit(bio_mtx);
}

static void
biothread(void *arg)
{

	rumpuser__hyp.hyp_schedule();
	rumpuser__hyp.hyp_lwproc_newlwp(0);
	rumpuser__hyp.hyp_unschedule();

	for (;;) {
		rumpuser_mutex_enter_nowrap(bio_mtx);
		while (bio_outstanding == 0)
			rumpuser_cv_wait_nowrap(bio_cv, bio_mtx);
		rumpuser_mutex_exit(bio_mtx);

		blkfront_aio_poll(blkdevs[0]);
	}
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

	rumpuser_mutex_enter(bio_mtx);
	bio_outstanding++;
	rumpuser_cv_signal(bio_cv);
	rumpuser_mutex_exit(bio_mtx);

	rumpkern_sched(nlocks, NULL);
}

void
rumpuser_seterrno(int err)
{

	get_current()->threrrno = err;
}
