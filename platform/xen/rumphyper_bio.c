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
#include <mini-os/time.h>

#include <mini-os/blkfront.h>
#include <mini-os/mm.h>

#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>

#include <bmk-core/errno.h>
#include <bmk-core/memalloc.h>
#include <bmk-core/sched.h>
#include <bmk-core/string.h>

#include <bmk-rumpuser/rumpuser.h>

static struct rumpuser_mtx *bio_mtx;
static struct rumpuser_cv *bio_cv;
static int bio_outstanding_total;

#define NBLKDEV 10
#define BLKFDOFF 64
static struct blkfront_dev *blkdevs[NBLKDEV];
static struct blkfront_info blkinfos[NBLKDEV];
static int blkopen[NBLKDEV];
static int blkdev_outstanding[NBLKDEV];

/* not really bio-specific, but only touches this file for now */
int
rumprun_platform_rumpuser_init(void)
{

	rumpuser_mutex_init(&bio_mtx, RUMPUSER_MTX_SPIN);
	rumpuser_cv_init(&bio_cv);

	return 0;
}

static int
devopen(int num)
{
	int devnum = 768 + (num<<6);
	char buf[32];
	int nlocks;

	if (blkopen[num]) {
		blkopen[num]++;
		return 1;
	}

	snprintf(buf, sizeof(buf), "device/vbd/%d", devnum);

	rumpkern_unsched(&nlocks, NULL);
	blkdevs[num] = blkfront_init(buf, &blkinfos[num]);
	rumpkern_sched(nlocks, NULL);

	if (blkdevs[num] != NULL) {
		blkopen[num] = 1;
		return 0;
	} else {
		return BMK_EIO; /* guess something */
	}
}

static int
devname2num(const char *name)
{
	const char *p;
	int num;

	/* we support only block devices */
	if (bmk_strncmp(name, "blk", 3) != 0 || bmk_strlen(name) != 4)
		return -1;

	p = name + bmk_strlen(name)-1;
	num = *p - '0';
	if (num < 0 || num >= NBLKDEV)
		return -1;

	return num;
}

int
rumpuser_open(const char *name, int mode, int *fdp)
{
	int acc, rv, num;

	if ((mode & RUMPUSER_OPEN_BIO) == 0 || (num = devname2num(name)) == -1)
		return BMK_ENXIO;

	if ((rv = devopen(num)) != 0)
		return rv;

	acc = mode & RUMPUSER_OPEN_ACCMODE;
	if (acc == RUMPUSER_OPEN_WRONLY || acc == RUMPUSER_OPEN_RDWR) {
		if (blkinfos[num].mode != O_RDWR) {
			/* XXX: unopen */
			return BMK_EROFS;
		}
	}

	*fdp = BLKFDOFF + num;
	return 0;
}

int
rumpuser_close(int fd)
{
	int rfd = fd - BLKFDOFF;

	if (rfd < 0 || rfd+1 > NBLKDEV)
		return BMK_EBADF;

	if (--blkopen[rfd] == 0) {
		struct blkfront_dev *toclose = blkdevs[rfd];
		
		/* not sure if this appropriately prevents races either ... */
		blkdevs[rfd] = NULL;
		blkfront_shutdown(toclose);
	}

	return 0;
}

int
rumpuser_getfileinfo(const char *name, uint64_t *size, int *type)
{
	int rv, num;

	if ((num = devname2num(name)) == -1)
		return BMK_ENXIO;
	if ((rv = devopen(num)) != 0)
		return rv;

	*size = blkinfos[num].sectors * blkinfos[num].sector_size;
	*type = RUMPUSER_FT_BLK;

	rumpuser_close(num + BLKFDOFF);

	return 0;
}

struct biocb {
	struct blkfront_aiocb bio_aiocb;
	int bio_num;
	rump_biodone_fn bio_done;
	void *bio_arg;
};

static void
biocomp(struct blkfront_aiocb *aiocb, int ret)
{
	struct biocb *bio = aiocb->data;
	int dummy, num;

	rumpkern_sched(0, NULL);
	if (ret)
		bio->bio_done(bio->bio_arg, 0, BMK_EIO);
	else
		bio->bio_done(bio->bio_arg, bio->bio_aiocb.aio_nbytes, 0);
	rumpkern_unsched(&dummy, NULL);
	num = bio->bio_num;
	bmk_memfree(bio);

	rumpuser_mutex_enter_nowrap(bio_mtx);
	bio_outstanding_total--;
	blkdev_outstanding[num]--;
	rumpuser_mutex_exit(bio_mtx);
}

static void
biothread(void *arg)
{
	DEFINE_WAIT(w);
	int i, flags, did;

	/* for the bio callback */
	rumpuser__hyp.hyp_schedule();
	rumpuser__hyp.hyp_lwproc_newlwp(0);
	rumpuser__hyp.hyp_unschedule();

	for (;;) {
		rumpuser_mutex_enter_nowrap(bio_mtx);
		while (bio_outstanding_total == 0) {
			rumpuser_cv_wait_nowrap(bio_cv, bio_mtx);
		}
		rumpuser_mutex_exit(bio_mtx);

		/*
		 * if we made any progress, recheck.  could be batched,
		 * but since currently locks are free here ... meh
		 */
		local_irq_save(flags);
		for (did = 0;;) {
			for (i = 0; i < NBLKDEV; i++) {
				if (blkdev_outstanding[i])
					did += blkfront_aio_poll(blkdevs[i]);
			}
			if (did)
				break;
			minios_add_waiter(w, blkfront_queue);
			local_irq_restore(flags);
			bmk_sched();
			local_irq_save(flags);
		}
		local_irq_restore(flags);
	}
}

void
rumpuser_bio(int fd, int op, void *data, size_t dlen, int64_t off,
	rump_biodone_fn biodone, void *donearg)
{
	static int bio_inited;
	struct biocb *bio = bmk_memalloc(sizeof(*bio), 0);
	struct blkfront_aiocb *aiocb = &bio->bio_aiocb;
	int nlocks;
	int num = fd - BLKFDOFF;

	rumpkern_unsched(&nlocks, NULL);

	if (!bio_inited) {
		rumpuser_mutex_enter_nowrap(bio_mtx);
		if (!bio_inited) {
			bio_inited = 1;
			rumpuser_mutex_exit(bio_mtx);
			bmk_sched_create("biopoll", NULL, 0,
			    biothread, NULL, NULL, 0);
		} else {
			rumpuser_mutex_exit(bio_mtx);
		}
	}

	bio->bio_done = biodone;
	bio->bio_arg = donearg;
	bio->bio_num = num;

	aiocb->aio_dev = blkdevs[num];
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
	bio_outstanding_total++;
	blkdev_outstanding[num]++;
	rumpuser_cv_signal(bio_cv);
	rumpuser_mutex_exit(bio_mtx);

	rumpkern_sched(nlocks, NULL);
}
