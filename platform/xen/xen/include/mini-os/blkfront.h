#ifndef _MINIOS_BLKFRONT_H_
#define _MINIOS_BLKFRONT_H_

#include <mini-os/wait.h>
#include <xen/io/blkif.h>
#include <mini-os/types.h>
struct blkfront_dev;
struct blkfront_aiocb
{
    struct blkfront_dev *aio_dev;
    uint8_t *aio_buf;
    size_t aio_nbytes;
    int64_t aio_offset;
    size_t total_bytes;
    uint8_t is_write;
    void *data;

    grant_ref_t gref[BLKIF_MAX_SEGMENTS_PER_REQUEST];
    int n;

    void (*aio_cb)(struct blkfront_aiocb *aiocb, int ret);
};

enum blkfront_mode { BLKFRONT_RDONLY, BLKFRONT_RDWR };
struct blkfront_info
{
    uint64_t sectors;
    unsigned sector_size;
    int mode;
    enum blkfront_mode info;
    int barrier;
    int flush;
};
struct blkfront_dev *blkfront_init(char *nodename, struct blkfront_info *info);
void blkfront_aio(struct blkfront_aiocb *aiocbp, int write);
#define blkfront_aio_read(aiocbp) blkfront_aio(aiocbp, 0)
#define blkfront_aio_write(aiocbp) blkfront_aio(aiocbp, 1)
void blkfront_io(struct blkfront_aiocb *aiocbp, int write);
#define blkfront_read(aiocbp) blkfront_io(aiocbp, 0)
#define blkfront_write(aiocbp) blkfront_io(aiocbp, 1)
void blkfront_aio_push_operation(struct blkfront_aiocb *aiocbp, uint8_t op);
int blkfront_aio_poll(struct blkfront_dev *dev);
void blkfront_sync(struct blkfront_dev *dev);
void blkfront_shutdown(struct blkfront_dev *dev);

extern struct wait_queue_head blkfront_queue;

#endif /* _MINIOS_BLKFRONT_H_ */
