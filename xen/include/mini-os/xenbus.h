#ifndef MINIOS_XENBUS_H__
#define MINIOS_XENBUS_H__

#include <xen/io/xenbus.h>
#include <mini-os/sched.h>
#include <mini-os/waittypes.h>
#include <mini-os/queue.h>
#include <mini-os/spinlock.h>

typedef unsigned long xenbus_transaction_t;
#define XBT_NIL ((xenbus_transaction_t)0)

#ifdef CONFIG_XENBUS
/* Initialize the XenBus system. */
void init_xenbus(void);
#else
static inline void init_xenbus(void)
{
}
#endif

/* Read the value associated with a path.  Returns a malloc'd error
   string on failure and sets *value to NULL.  On success, *value is
   set to a malloc'd copy of the value. */
char *xenbus_read(xenbus_transaction_t xbt, const char *path, char **value);

/* All accesses to an active xenbus_event_queue must occur with this
 * lock held.  The public functions here will do that for you, but
 * your own accesses to the queue (including the contained waitq)
 * must be protected by the lock. */
extern spinlock_t xenbus_req_lock;

/* Queue for events (watches or async request replies - see below) */
struct xenbus_event {
    union {
        struct {
            /* must be first, both for the bare minios xs.c, and for
             * xenbus_wait_for_watch's handling */
            char *path;
            char *token;
        };
        struct xsd_sockmsg *reply;
    };
    struct xenbus_watch *watch;
    MINIOS_STAILQ_ENTRY(xenbus_event) entry;
};
struct xenbus_event_queue {
    MINIOS_STAILQ_HEAD(, xenbus_event) events;
    void (*wakeup)(struct xenbus_event_queue*); /* can be safely ignored */
    struct wait_queue_head waitq;
};

void xenbus_event_queue_init(struct xenbus_event_queue *queue);

char *xenbus_watch_path_token(xenbus_transaction_t xbt, const char *path, const char *token, struct xenbus_event_queue *events);
char *xenbus_unwatch_path_token(xenbus_transaction_t xbt, const char *path, const char *token);
void xenbus_wait_for_watch(struct xenbus_event_queue *queue);
char **xenbus_wait_for_watch_return(struct xenbus_event_queue *queue);
char* xenbus_wait_for_value(const char *path, const char *value, struct xenbus_event_queue *queue);
char *xenbus_wait_for_state_change(const char* path, XenbusState *state, struct xenbus_event_queue *queue);
char *xenbus_switch_state(xenbus_transaction_t xbt, const char* path, XenbusState state);

/* When no token is provided, use a global queue. */
#define XENBUS_WATCH_PATH_TOKEN "xenbus_watch_path"
extern struct xenbus_event_queue xenbus_default_watch_queue;
#define xenbus_watch_path(xbt, path) xenbus_watch_path_token(xbt, path, XENBUS_WATCH_PATH_TOKEN, NULL)
#define xenbus_unwatch_path(xbt, path) xenbus_unwatch_path_token(xbt, path, XENBUS_WATCH_PATH_TOKEN)


/* Associates a value with a path.  Returns a malloc'd error string on
   failure. */
char *xenbus_write(xenbus_transaction_t xbt, const char *path, const char *value);

struct write_req {
    const void *data;
    unsigned len;
};

/* Send a message to xenbus, in the same fashion as xb_write, and
   block waiting for a reply.  The reply is malloced and should be
   freed by the caller. */
struct xsd_sockmsg *
xenbus_msg_reply(int type,
                 xenbus_transaction_t trans,
                 struct write_req *io,
                 int nr_reqs);

/* Removes the value associated with a path.  Returns a malloc'd error
   string on failure. */
char *xenbus_rm(xenbus_transaction_t xbt, const char *path);

/* List the contents of a directory.  Returns a malloc'd error string
   on failure and sets *contents to NULL.  On success, *contents is
   set to a malloc'd array of pointers to malloc'd strings.  The array
   is NULL terminated.  May block. */
char *xenbus_ls(xenbus_transaction_t xbt, const char *prefix, char ***contents);

/* Reads permissions associated with a path.  Returns a malloc'd error
   string on failure and sets *value to NULL.  On success, *value is
   set to a malloc'd copy of the value. */
char *xenbus_get_perms(xenbus_transaction_t xbt, const char *path, char **value);

/* Sets the permissions associated with a path.  Returns a malloc'd
   error string on failure. */
char *xenbus_set_perms(xenbus_transaction_t xbt, const char *path, domid_t dom, char perm);

/* Start a xenbus transaction.  Returns the transaction in xbt on
   success or a malloc'd error string otherwise. */
char *xenbus_transaction_start(xenbus_transaction_t *xbt);

/* End a xenbus transaction.  Returns a malloc'd error string if it
   fails.  abort says whether the transaction should be aborted.
   Returns 1 in *retry iff the transaction should be retried. */
char *xenbus_transaction_end(xenbus_transaction_t, int abort,
			     int *retry);

/* Read path and parse it as an integer.  Returns -1 on error. */
int xenbus_read_integer(const char *path);

/* Contraction of snprintf and xenbus_write(path/node). */
char* xenbus_printf(xenbus_transaction_t xbt,
                                  const char* node, const char* path,
                                  const char* fmt, ...)
                   __attribute__((__format__(printf, 4, 5)));

/* Utility function to figure out our domain id */
domid_t xenbus_get_self_id(void);

/*
 * ----- asynchronous low-level interface -----
 */

/*
 * Use of queue->wakeup:
 *
 * If queue->wakeup is set, it will be called instead of
 * wake_up(&queue->waitq);
 *
 * queue->wakeup is initialised (to a function which just calls
 * wake_up) by xenbus_event_queue_init.  The user who wants something
 * different should set ->wakeup after the init, but before the queue
 * is used for xenbus_id_allocate or xenbus_watch_prepare.
 *
 * queue->wakeup() is called with the req_lock held.
 */

/* Allocate an identifier for a xenbus request.  Blocks if none are
 * available.  Cannot fail.  On return, we may use the returned value
 * as the id in a xenbus request.
 *
 * for_queue must already be allocated, but may be uninitialised.
 *
 * for_queue->watch is not touched by the xenbus machinery for
 * handling requests/replies but should probably be initialised by the
 * caller (probably to NULL) because this will help the caller
 * distinguish the reply from any watch events which might end up in
 * the same queue.
 *
 * reply_queue must exist and have been initialised.
 *
 * When the response arrives, the reply message will stored in
 * for_queue->reply and for_queue will be queued on reply_queue.  The
 * id must be then explicitly released (or, used again, if desired).
 * After ->reply is done with the caller must pass it to free().
 * (Do not use the id for more than one request at a time.) */
int xenbus_id_allocate(struct xenbus_event_queue *reply_queue,
                       struct xenbus_event *for_queue);
void xenbus_id_release(int id);

/* Allocating a token for a watch.
 *
 * To use this:
 *  - Include struct xenbus_watch in your own struct.
 *  - Set events; then call prepare.  This will set token.
 *    You may then use token in a WATCH request.
 *  - You must UNWATCH before you call release.
 * Do not modify token yourself.
 * entry is private for the xenbus driver.
 *
 * When the watch fires, a new struct xenbus_event will be allocated
 * and queued on events.  The field xenbus_event->watch will have been
 * set to watch by the xenbus machinery, and xenbus_event->path will
 * be the watch path.  After the caller is done with the event,
 * its pointer should simply be passed to free(). */
struct xenbus_watch {
    char *token;
    struct xenbus_event_queue *events;
    MINIOS_LIST_ENTRY(xenbus_watch) entry;
};
void xenbus_watch_init(struct xenbus_watch *watch); /* makes release a noop */
void xenbus_watch_prepare(struct xenbus_watch *watch); /* need not be init'd */
void xenbus_watch_release(struct xenbus_watch *watch); /* idempotent */


/* Send data to xenbus.  This can block.  All of the requests are seen
 * by xenbus as if sent atomically.  The header is added
 * automatically, using type %type, req_id %req_id, and trans_id
 * %trans_id. */
void xenbus_xb_write(int type, int req_id, xenbus_transaction_t trans_id,
		     const struct write_req *req, int nr_reqs);

void xenbus_free(void*);
/* If the caller is in a scope which uses a different malloc arena,
 * it must use this rather than free() when freeing data received
 * from xenbus. */

#ifdef CONFIG_XENBUS
/* Reset the XenBus system. */
void fini_xenbus(void);
#else
static inline void fini_xenbus(void)
{
}
#endif

#endif /* MINIOS_XENBUS_H__ */
