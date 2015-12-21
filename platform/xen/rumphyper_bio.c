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

#include <bmk-core/errno.h>
#include <bmk-core/memalloc.h>
#include <bmk-core/printf.h>
#include <bmk-core/sched.h>
#include <bmk-core/string.h>

#include <bmk-rumpuser/core_types.h>
#include <bmk-rumpuser/rumpuser.h>

static struct rumpuser_mtx *bio_mtx;
static struct rumpuser_cv *bio_cv;
static int bio_outstanding_total;

#define NBLKDEV 10
#define BLKFDOFF 64

static struct blkdev {
	struct blkfront_dev *blk_dev;
	struct blkfront_info blk_info;
	int blk_open;
	int blk_outstanding;
	int blk_vbd;
} blkdevs[NBLKDEV];

/* not really bio-specific, but only touches this file for now */
int
rumprun_platform_rumpuser_init(void)
{
	int i;

	rumpuser_mutex_init(&bio_mtx, RUMPUSER_MTX_SPIN);
	rumpuser_cv_init(&bio_cv);

	for (i = 0; i < NBLKDEV; i++) {
		blkdevs[i].blk_vbd = -1;
	}

	return 0;
}

static int
devopen(int num)
{
	bmk_assert(num < NBLKDEV);
	struct blkdev *bd = &blkdevs[num];
	char buf[32];
	int nlocks;

	if (bd->blk_open) {
		bd->blk_open++;
		return 1;
	}
	bmk_snprintf(buf, sizeof(buf), "device/vbd/%d", bd->blk_vbd);

	rumpkern_unsched(&nlocks, NULL);
	bd->blk_dev = blkfront_init(buf, &bd->blk_info);
	rumpkern_sched(nlocks, NULL);

	if (bd->blk_dev != NULL) {
		bd->blk_open = 1;
		return 0;
	} else {
		return BMK_EIO; /* guess something */
	}
}

/*
 * Translate block device spec into vbd id.
 * We parse up to "(hd|sd|xvd)[a-z][0-9]?", i.e. max 1 char disk/part.
 * (Feel free to improve)
 */
enum devtype { DEV_SD, DEV_HD, DEV_XVD };
#define XENBLK_MAGIC "blkfront:"
static int
devname2vbd(const char *name)
{
	const char *dp;
	enum devtype dt;
	int disk, part;
	int vbd;

	/* Do not print anything for clearly incorrect paths */
	if (bmk_strncmp(name, XENBLK_MAGIC, sizeof(XENBLK_MAGIC)-1) != 0)
		return -1;
	name += sizeof(XENBLK_MAGIC)-1;

	/* which type of disk? */
	if (bmk_strncmp(name, "hd", 2) == 0) {
		dp = name+2;
		dt = DEV_HD;
	} else if (bmk_strncmp(name, "sd", 2) == 0) {
		dp = name+2;
		dt = DEV_SD;
	} else if (bmk_strncmp(name, "xvd", 3) == 0) {
		dp = name+3;
		dt = DEV_XVD;
	} else {
		bmk_printf("unsupported devtype %s\n", name);
		return -1;
	}
	if (bmk_strlen(dp) < 1 || bmk_strlen(dp) > 2) {
		bmk_printf("unsupported blkspec %s\n", name);
		return -1;
	}

	/* disk and partition */
	disk = *dp - 'a';
	dp++;
	if (*dp == '\0')
		part = 0;
	else
		part = *dp - '0';
	if (disk < 0 || part < 0 || part > 9) {
		bmk_printf("unsupported disk/partition %d %d\n", disk, part);
		return -1;
	}

	/* construct vbd based on disk type */
	switch (dt) {
	case DEV_HD:
		if (disk < 2) {
			vbd = (3<<8) | (disk<<6) | part;
		} else if (disk < 4) {
			vbd = (22<<8) | ((disk-2)<<6) | part;
		} else {
			goto err;
		}
		break;
	case DEV_SD:
		if (disk > 16 || part > 16)
			goto err;
		vbd = (8<<8) | (disk<<4) | part;
		break;
	case DEV_XVD:
		if (disk < 16)
			vbd = (202<<8) | (disk<<4) | part;
		else if (disk > 'z' - 'a')
			goto err;
		else
			vbd = (1<<28) | (disk<<8) | part;
		break;
	}

	return vbd;
 err:
	bmk_printf("unsupported disk/partition spec %s\n", name);
	return -1;
}

static int
devname2num(const char *name)
{
	int vbd;
	int i;

	if ((vbd = devname2vbd(name)) == -1)
		return -1;

	/*
	 * We got a valid vbd.  Check if we know this one already, or
	 * if we need to reserve a new one.
	 */
	for (i = 0; i < NBLKDEV; i++) {
		if (vbd == blkdevs[i].blk_vbd)
			return i;
	}

	/*
	 * No such luck.  Reserve a new one
	 */
	for (i = 0; i < NBLKDEV; i++) {
		if (blkdevs[i].blk_vbd == -1) {
			/* i have you now */
			blkdevs[i].blk_vbd = vbd;
			return i;
		}
	}

	bmk_printf("blkdev table full.  Increase NBLKDEV\n");
	return -1;
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
		struct blkdev *bd = &blkdevs[num];
		if (bd->blk_info.mode != BLKFRONT_RDWR) {
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
	struct blkdev *bd;

	if (rfd < 0 || rfd+1 > NBLKDEV)
		return BMK_EBADF;

	bd = &blkdevs[rfd];
	if (--bd->blk_open == 0) {
		struct blkfront_dev *toclose = bd->blk_dev;
		
		/* not sure if this appropriately prevents races either ... */
		bd->blk_dev = NULL;
		blkfront_shutdown(toclose);
	}

	return 0;
}

int
rumpuser_getfileinfo(const char *name, uint64_t *size, int *type)
{
	struct blkdev *bd;
	int rv, num;

	if ((num = devname2num(name)) == -1)
		return BMK_ENXIO;
	if ((rv = devopen(num)) != 0)
		return rv;

	bd = &blkdevs[num];
	*size = bd->blk_info.sectors * bd->blk_info.sector_size;
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
	bmk_memfree(bio, BMK_MEMWHO_WIREDBMK);

	rumpuser_mutex_enter_nowrap(bio_mtx);
	bio_outstanding_total--;
	blkdevs[num].blk_outstanding--;
	rumpuser_mutex_exit(bio_mtx);
}

static void
biothread(void *arg)
{
	DEFINE_WAIT(w);
	int flags, did;

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
			struct blkdev *bd;

			for (bd = &blkdevs[0]; bd < &blkdevs[NBLKDEV]; bd++) {
				if (bd->blk_outstanding)
					did += blkfront_aio_poll(bd->blk_dev);
			}
			if (did)
				break;
			minios_add_waiter(w, blkfront_queue);
			local_irq_restore(flags);
			minios_wait(w);
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
	struct biocb *bio = bmk_xmalloc_bmk(sizeof(*bio));
	struct blkfront_aiocb *aiocb = &bio->bio_aiocb;
	int nlocks;
	int num = fd - BLKFDOFF;
	struct blkdev *bd = &blkdevs[num];

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

	aiocb->aio_dev = bd->blk_dev;
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
	bd->blk_outstanding++;
	rumpuser_cv_signal(bio_cv);
	rumpuser_mutex_exit(bio_mtx);

	rumpkern_sched(nlocks, NULL);
}
