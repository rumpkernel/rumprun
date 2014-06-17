/* 
 ****************************************************************************
 * (C) 2006 - Cambridge University
 ****************************************************************************
 *
 *        File: xenbus.c
 *      Author: Steven Smith (sos22@cam.ac.uk) 
 *     Changes: Grzegorz Milos (gm281@cam.ac.uk)
 *     Changes: John D. Ramsdell
 *              
 *        Date: Jun 2006, chages Aug 2005
 * 
 * Environment: Xen Minimal OS
 * Description: Minimal implementation of xenbus
 *
 ****************************************************************************
 **/
#include <mini-os/os.h>
#include <mini-os/mm.h>
#include <mini-os/lib.h>
#include <mini-os/xenbus.h>
#include <mini-os/events.h>
#include <mini-os/sched.h>
#include <mini-os/wait.h>
#include <xen/io/xs_wire.h>
#include <mini-os/spinlock.h>
#include <mini-os/xmalloc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define min(x,y) ({                       \
        typeof(x) tmpx = (x);                 \
        typeof(y) tmpy = (y);                 \
        tmpx < tmpy ? tmpx : tmpy;            \
        })

#ifdef XENBUS_DEBUG
#define DEBUG(_f, _a...) \
    printk("MINI_OS(file=xenbus.c, line=%d) " _f , __LINE__, ## _a)
#else
#define DEBUG(_f, _a...)    ((void)0)
#endif

static struct xenstore_domain_interface *xenstore_buf;
static DECLARE_WAIT_QUEUE_HEAD(xb_waitq);
static spinlock_t xb_lock = SPIN_LOCK_UNLOCKED; /* protects xenbus req ring */

struct xenbus_event_queue xenbus_default_watch_queue;
static MINIOS_LIST_HEAD(, xenbus_watch) watches;
struct xenbus_req_info 
{
    struct xenbus_event_queue *reply_queue; /* non-0 iff in use */
    struct xenbus_event *for_queue;
};


spinlock_t xenbus_req_lock = SPIN_LOCK_UNLOCKED;
/*
 * This lock protects:
 *    the xenbus request ring
 *    req_info[]
 *    all live struct xenbus_event_queue (including xenbus_default_watch_queue)
 *    nr_live_reqs
 *    req_wq
 *    watches
 */

static void queue_wakeup(struct xenbus_event_queue *queue)
{
    wake_up(&queue->waitq);
}

void xenbus_event_queue_init(struct xenbus_event_queue *queue)
{
    MINIOS_STAILQ_INIT(&queue->events);
    queue->wakeup = queue_wakeup;
    init_waitqueue_head(&queue->waitq);
}

static struct xenbus_event *remove_event(struct xenbus_event_queue *queue)
{
    /* Called with lock held */
    struct xenbus_event *event;

    event = MINIOS_STAILQ_FIRST(&queue->events);
    if (!event)
        goto out;
    MINIOS_STAILQ_REMOVE_HEAD(&queue->events, entry);

 out:
    return event;
}

static void queue_event(struct xenbus_event_queue *queue,
                        struct xenbus_event *event)
{
    /* Called with lock held */
    MINIOS_STAILQ_INSERT_TAIL(&queue->events, event, entry);
    queue->wakeup(queue);
}

static struct xenbus_event *await_event(struct xenbus_event_queue *queue)
{
    struct xenbus_event *event;
    DEFINE_WAIT(w);
    spin_lock(&xenbus_req_lock);
    while (!(event = remove_event(queue))) {
        add_waiter(w, queue->waitq);
        spin_unlock(&xenbus_req_lock);
        schedule();
        spin_lock(&xenbus_req_lock);
    }
    remove_waiter(w, queue->waitq);
    spin_unlock(&xenbus_req_lock);
    return event;
}


#define NR_REQS 32
static struct xenbus_req_info req_info[NR_REQS];

static void memcpy_from_ring(const void *Ring,
        void *Dest,
        int off,
        int len)
{
    int c1, c2;
    const char *ring = Ring;
    char *dest = Dest;
    c1 = min(len, XENSTORE_RING_SIZE - off);
    c2 = len - c1;
    memcpy(dest, ring + off, c1);
    memcpy(dest + c1, ring, c2);
}

char **xenbus_wait_for_watch_return(struct xenbus_event_queue *queue)
{
    struct xenbus_event *event;
    if (!queue)
        queue = &xenbus_default_watch_queue;
    event = await_event(queue);
    return &event->path;
}

void xenbus_wait_for_watch(struct xenbus_event_queue *queue)
{
    char **ret;
    if (!queue)
        queue = &xenbus_default_watch_queue;
    ret = xenbus_wait_for_watch_return(queue);
    if (ret)
        free(ret);
    else
        printk("unexpected path returned by watch\n");
}

char* xenbus_wait_for_value(const char* path, const char* value, struct xenbus_event_queue *queue)
{
    if (!queue)
        queue = &xenbus_default_watch_queue;
    for(;;)
    {
        char *res, *msg;
        int r;

        msg = xenbus_read(XBT_NIL, path, &res);
        if(msg) return msg;

        r = strcmp(value,res);
        free(res);

        if(r==0) break;
        else xenbus_wait_for_watch(queue);
    }
    return NULL;
}

char *xenbus_switch_state(xenbus_transaction_t xbt, const char* path, XenbusState state)
{
    char *current_state;
    char *msg = NULL;
    char *msg2 = NULL;
    char value[2];
    XenbusState rs;
    int xbt_flag = 0;
    int retry = 0;

    do {
        if (xbt == XBT_NIL) {
            msg = xenbus_transaction_start(&xbt);
            if (msg) goto exit;
            xbt_flag = 1;
        }

        msg = xenbus_read(xbt, path, &current_state);
        if (msg) goto exit;

        rs = (XenbusState) (current_state[0] - '0');
        free(current_state);
        if (rs == state) {
            msg = NULL;
            goto exit;
        }

        snprintf(value, 2, "%d", state);
        msg = xenbus_write(xbt, path, value);

exit:
        if (xbt_flag) {
            msg2 = xenbus_transaction_end(xbt, 0, &retry);
            xbt = XBT_NIL;
        }
        if (msg == NULL && msg2 != NULL)
            msg = msg2;
    } while (retry);

    return msg;
}

char *xenbus_wait_for_state_change(const char* path, XenbusState *state, struct xenbus_event_queue *queue)
{
    if (!queue)
        queue = &xenbus_default_watch_queue;
    for(;;)
    {
        char *res, *msg;
        XenbusState rs;

        msg = xenbus_read(XBT_NIL, path, &res);
        if(msg) return msg;

        rs = (XenbusState) (res[0] - 48);
        free(res);

        if (rs == *state)
            xenbus_wait_for_watch(queue);
        else {
            *state = rs;
            break;
        }
    }
    return NULL;
}


static void xenbus_thread_func(void *ign)
{
    struct xsd_sockmsg msg;
    unsigned prod = xenstore_buf->rsp_prod;

    for (;;) 
    {
        wait_event(xb_waitq, prod != xenstore_buf->rsp_prod);
        while (1) 
        {
            prod = xenstore_buf->rsp_prod;
            DEBUG("Rsp_cons %d, rsp_prod %d.\n", xenstore_buf->rsp_cons,
                    xenstore_buf->rsp_prod);
            if (xenstore_buf->rsp_prod - xenstore_buf->rsp_cons < sizeof(msg))
                break;
            rmb();
            memcpy_from_ring(xenstore_buf->rsp,
                    &msg,
                    MASK_XENSTORE_IDX(xenstore_buf->rsp_cons),
                    sizeof(msg));
            DEBUG("Msg len %d, %d avail, id %d.\n",
                    msg.len + sizeof(msg),
                    xenstore_buf->rsp_prod - xenstore_buf->rsp_cons,
                    msg.req_id);
            if (xenstore_buf->rsp_prod - xenstore_buf->rsp_cons <
                    sizeof(msg) + msg.len)
                break;

            DEBUG("Message is good.\n");

            if(msg.type == XS_WATCH_EVENT)
            {
		struct xenbus_event *event = malloc(sizeof(*event) + msg.len);
                struct xenbus_event_queue *events = NULL;
		char *data = (char*)event + sizeof(*event);
                struct xenbus_watch *watch;

                memcpy_from_ring(xenstore_buf->rsp,
		    data,
                    MASK_XENSTORE_IDX(xenstore_buf->rsp_cons + sizeof(msg)),
                    msg.len);

		event->path = data;
		event->token = event->path + strlen(event->path) + 1;

                xenstore_buf->rsp_cons += msg.len + sizeof(msg);

                spin_lock(&xenbus_req_lock);

                MINIOS_LIST_FOREACH(watch, &watches, entry)
                    if (!strcmp(watch->token, event->token)) {
                        event->watch = watch;
                        events = watch->events;
                        break;
                    }

                if (events) {
                    queue_event(events, event);
                } else {
                    printk("unexpected watch token %s\n", event->token);
                    free(event);
                }

                spin_unlock(&xenbus_req_lock);
            }

            else
            {
                req_info[msg.req_id].for_queue->reply =
                    malloc(sizeof(msg) + msg.len);
                memcpy_from_ring(xenstore_buf->rsp,
                    req_info[msg.req_id].for_queue->reply,
                    MASK_XENSTORE_IDX(xenstore_buf->rsp_cons),
                    msg.len + sizeof(msg));
                xenstore_buf->rsp_cons += msg.len + sizeof(msg);
                spin_lock(&xenbus_req_lock);
                queue_event(req_info[msg.req_id].reply_queue,
                            req_info[msg.req_id].for_queue);
                spin_unlock(&xenbus_req_lock);
            }
        }
    }
}

static void xenbus_evtchn_handler(evtchn_port_t port, struct pt_regs *regs,
				  void *ign)
{
    wake_up(&xb_waitq);
}

static int nr_live_reqs;
static DECLARE_WAIT_QUEUE_HEAD(req_wq);

/* Release a xenbus identifier */
void xenbus_id_release(int id)
{
    BUG_ON(!req_info[id].reply_queue);
    spin_lock(&xenbus_req_lock);
    req_info[id].reply_queue = 0;
    nr_live_reqs--;
    if (nr_live_reqs == NR_REQS - 1)
        wake_up(&req_wq);
    spin_unlock(&xenbus_req_lock);
}

int xenbus_id_allocate(struct xenbus_event_queue *reply_queue,
                       struct xenbus_event *for_queue)
{
    static int probe;
    int o_probe;

    while (1) 
    {
        spin_lock(&xenbus_req_lock);
        if (nr_live_reqs < NR_REQS)
            break;
        spin_unlock(&xenbus_req_lock);
        wait_event(req_wq, (nr_live_reqs < NR_REQS));
    }

    o_probe = probe;
    for (;;) 
    {
        if (!req_info[o_probe].reply_queue)
            break;
        o_probe = (o_probe + 1) % NR_REQS;
        BUG_ON(o_probe == probe);
    }
    nr_live_reqs++;
    req_info[o_probe].reply_queue = reply_queue;
    req_info[o_probe].for_queue = for_queue;
    probe = (o_probe + 1) % NR_REQS;
    spin_unlock(&xenbus_req_lock);

    return o_probe;
}

void xenbus_watch_init(struct xenbus_watch *watch)
{
    watch->token = 0;
}

void xenbus_watch_prepare(struct xenbus_watch *watch)
{
    BUG_ON(!watch->events);
    size_t size = sizeof(void*)*2 + 5;
    watch->token = malloc(size);
    int r = snprintf(watch->token,size,"*%p",(void*)watch);
    BUG_ON(!(r > 0 && r < size));
    spin_lock(&xenbus_req_lock);
    MINIOS_LIST_INSERT_HEAD(&watches, watch, entry);
    spin_unlock(&xenbus_req_lock);
}

void xenbus_watch_release(struct xenbus_watch *watch)
{
    if (!watch->token)
        return;
    spin_lock(&xenbus_req_lock);
    MINIOS_LIST_REMOVE(watch, entry);
    spin_unlock(&xenbus_req_lock);
    free(watch->token);
    watch->token = 0;
}

/* Initialise xenbus. */
void init_xenbus(void)
{
    int err;
    DEBUG("init_xenbus called.\n");
    xenbus_event_queue_init(&xenbus_default_watch_queue);
    xenstore_buf = mfn_to_virt(start_info.store_mfn);
    create_thread("xenstore", xenbus_thread_func, NULL);
    DEBUG("buf at %p.\n", xenstore_buf);
    err = bind_evtchn(start_info.store_evtchn,
		      xenbus_evtchn_handler,
              NULL);
    unmask_evtchn(start_info.store_evtchn);
    printk("xenbus initialised on irq %d mfn %#lx\n",
	   err, start_info.store_mfn);
}

void fini_xenbus(void)
{
}

void xenbus_xb_write(int type, int req_id, xenbus_transaction_t trans_id,
		     const struct write_req *req, int nr_reqs)
{
    XENSTORE_RING_IDX prod;
    int r;
    int len = 0;
    const struct write_req *cur_req;
    int req_off;
    int total_off;
    int this_chunk;
    struct xsd_sockmsg m = {.type = type, .req_id = req_id,
        .tx_id = trans_id };
    struct write_req header_req = { &m, sizeof(m) };

    for (r = 0; r < nr_reqs; r++)
        len += req[r].len;
    m.len = len;
    len += sizeof(m);

    cur_req = &header_req;

    BUG_ON(len > XENSTORE_RING_SIZE);

    spin_lock(&xb_lock);
    /* Wait for the ring to drain to the point where we can send the
       message. */
    prod = xenstore_buf->req_prod;
    if (prod + len - xenstore_buf->req_cons > XENSTORE_RING_SIZE) 
    {
        /* Wait for there to be space on the ring */
        DEBUG("prod %d, len %d, cons %d, size %d; waiting.\n",
                prod, len, xenstore_buf->req_cons, XENSTORE_RING_SIZE);
        spin_unlock(&xb_lock);
        wait_event(xb_waitq,
                xenstore_buf->req_prod + len - xenstore_buf->req_cons <=
                XENSTORE_RING_SIZE);
        spin_lock(&xb_lock);
        DEBUG("Back from wait.\n");
        prod = xenstore_buf->req_prod;
    }

    /* We're now guaranteed to be able to send the message without
       overflowing the ring.  Do so. */
    total_off = 0;
    req_off = 0;
    while (total_off < len) 
    {
        this_chunk = min(cur_req->len - req_off,
                XENSTORE_RING_SIZE - MASK_XENSTORE_IDX(prod));
        memcpy((char *)xenstore_buf->req + MASK_XENSTORE_IDX(prod),
                (char *)cur_req->data + req_off, this_chunk);
        prod += this_chunk;
        req_off += this_chunk;
        total_off += this_chunk;
        if (req_off == cur_req->len) 
        {
            req_off = 0;
            if (cur_req == &header_req)
                cur_req = req;
            else
                cur_req++;
        }
    }

    DEBUG("Complete main loop of xb_write.\n");
    BUG_ON(req_off != 0);
    BUG_ON(total_off != len);
    BUG_ON(prod > xenstore_buf->req_cons + XENSTORE_RING_SIZE);

    /* Remote must see entire message before updating indexes */
    wmb();

    xenstore_buf->req_prod += len;
    spin_unlock(&xb_lock);

    /* Send evtchn to notify remote */
    notify_remote_via_evtchn(start_info.store_evtchn);
}

/* Send a mesasge to xenbus, in the same fashion as xb_write, and
   block waiting for a reply.  The reply is malloced and should be
   freed by the caller. */
struct xsd_sockmsg *
xenbus_msg_reply(int type,
		 xenbus_transaction_t trans,
		 struct write_req *io,
		 int nr_reqs)
{
    int id;
    struct xsd_sockmsg *rep;
    struct xenbus_event_queue queue;
    struct xenbus_event event_buf;

    xenbus_event_queue_init(&queue);

    id = xenbus_id_allocate(&queue,&event_buf);

    xenbus_xb_write(type, id, trans, io, nr_reqs);

    struct xenbus_event *event = await_event(&queue);
    BUG_ON(event != &event_buf);

    rep = req_info[id].for_queue->reply;
    BUG_ON(rep->req_id != id);
    xenbus_id_release(id);
    return rep;
}

static char *errmsg(struct xsd_sockmsg *rep)
{
    char *res;
    if (!rep) {
	char msg[] = "No reply";
	size_t len = strlen(msg) + 1;
	return memcpy(malloc(len), msg, len);
    }
    if (rep->type != XS_ERROR)
	return NULL;
    res = malloc(rep->len + 1);
    memcpy(res, rep + 1, rep->len);
    res[rep->len] = 0;
    free(rep);
    return res;
}	

/* Send a debug message to xenbus.  Can block. */
static void xenbus_debug_msg(const char *msg)
{
    int len = strlen(msg);
    struct write_req req[] = {
        { "print", sizeof("print") },
        { msg, len },
        { "", 1 }};
    struct xsd_sockmsg *reply;

    reply = xenbus_msg_reply(XS_DEBUG, 0, req, ARRAY_SIZE(req));
    printk("Got a reply, type %d, id %d, len %d.\n",
            reply->type, reply->req_id, reply->len);
}

/* List the contents of a directory.  Returns a malloc()ed array of
   pointers to malloc()ed strings.  The array is NULL terminated.  May
   block. */
char *xenbus_ls(xenbus_transaction_t xbt, const char *pre, char ***contents)
{
    struct xsd_sockmsg *reply, *repmsg;
    struct write_req req[] = { { pre, strlen(pre)+1 } };
    int nr_elems, x, i;
    char **res, *msg;

    repmsg = xenbus_msg_reply(XS_DIRECTORY, xbt, req, ARRAY_SIZE(req));
    msg = errmsg(repmsg);
    if (msg) {
	*contents = NULL;
	return msg;
    }
    reply = repmsg + 1;
    for (x = nr_elems = 0; x < repmsg->len; x++)
        nr_elems += (((char *)reply)[x] == 0);
    res = malloc(sizeof(res[0]) * (nr_elems + 1));
    for (x = i = 0; i < nr_elems; i++) {
        int l = strlen((char *)reply + x);
        res[i] = malloc(l + 1);
        memcpy(res[i], (char *)reply + x, l + 1);
        x += l + 1;
    }
    res[i] = NULL;
    free(repmsg);
    *contents = res;
    return NULL;
}

char *xenbus_read(xenbus_transaction_t xbt, const char *path, char **value)
{
    struct write_req req[] = { {path, strlen(path) + 1} };
    struct xsd_sockmsg *rep;
    char *res, *msg;
    rep = xenbus_msg_reply(XS_READ, xbt, req, ARRAY_SIZE(req));
    msg = errmsg(rep);
    if (msg) {
	*value = NULL;
	return msg;
    }
    res = malloc(rep->len + 1);
    memcpy(res, rep + 1, rep->len);
    res[rep->len] = 0;
    free(rep);
    *value = res;
    return NULL;
}

char *xenbus_write(xenbus_transaction_t xbt, const char *path, const char *value)
{
    struct write_req req[] = { 
	{path, strlen(path) + 1},
	{value, strlen(value)},
    };
    struct xsd_sockmsg *rep;
    char *msg;
    rep = xenbus_msg_reply(XS_WRITE, xbt, req, ARRAY_SIZE(req));
    msg = errmsg(rep);
    if (msg) return msg;
    free(rep);
    return NULL;
}

char* xenbus_watch_path_token( xenbus_transaction_t xbt, const char *path, const char *token, struct xenbus_event_queue *events)
{
    struct xsd_sockmsg *rep;

    struct write_req req[] = { 
        {path, strlen(path) + 1},
	{token, strlen(token) + 1},
    };

    struct xenbus_watch *watch = malloc(sizeof(*watch));

    char *msg;

    if (!events)
        events = &xenbus_default_watch_queue;

    watch->token = strdup(token);
    watch->events = events;

    spin_lock(&xenbus_req_lock);
    MINIOS_LIST_INSERT_HEAD(&watches, watch, entry);
    spin_unlock(&xenbus_req_lock);

    rep = xenbus_msg_reply(XS_WATCH, xbt, req, ARRAY_SIZE(req));

    msg = errmsg(rep);
    if (msg) return msg;
    free(rep);

    return NULL;
}

char* xenbus_unwatch_path_token( xenbus_transaction_t xbt, const char *path, const char *token)
{
    struct xsd_sockmsg *rep;

    struct write_req req[] = { 
        {path, strlen(path) + 1},
	{token, strlen(token) + 1},
    };

    struct xenbus_watch *watch;

    char *msg;

    rep = xenbus_msg_reply(XS_UNWATCH, xbt, req, ARRAY_SIZE(req));

    msg = errmsg(rep);
    if (msg) return msg;
    free(rep);

    spin_lock(&xenbus_req_lock);
    MINIOS_LIST_FOREACH(watch, &watches, entry)
        if (!strcmp(watch->token, token)) {
            free(watch->token);
            MINIOS_LIST_REMOVE(watch, entry);
            free(watch);
            break;
        }
    spin_unlock(&xenbus_req_lock);

    return NULL;
}

char *xenbus_rm(xenbus_transaction_t xbt, const char *path)
{
    struct write_req req[] = { {path, strlen(path) + 1} };
    struct xsd_sockmsg *rep;
    char *msg;
    rep = xenbus_msg_reply(XS_RM, xbt, req, ARRAY_SIZE(req));
    msg = errmsg(rep);
    if (msg)
	return msg;
    free(rep);
    return NULL;
}

char *xenbus_get_perms(xenbus_transaction_t xbt, const char *path, char **value)
{
    struct write_req req[] = { {path, strlen(path) + 1} };
    struct xsd_sockmsg *rep;
    char *res, *msg;
    rep = xenbus_msg_reply(XS_GET_PERMS, xbt, req, ARRAY_SIZE(req));
    msg = errmsg(rep);
    if (msg) {
	*value = NULL;
	return msg;
    }
    res = malloc(rep->len + 1);
    memcpy(res, rep + 1, rep->len);
    res[rep->len] = 0;
    free(rep);
    *value = res;
    return NULL;
}

#define PERM_MAX_SIZE 32
char *xenbus_set_perms(xenbus_transaction_t xbt, const char *path, domid_t dom, char perm)
{
    char value[PERM_MAX_SIZE];
    struct write_req req[] = { 
	{path, strlen(path) + 1},
	{value, 0},
    };
    struct xsd_sockmsg *rep;
    char *msg;
    snprintf(value, PERM_MAX_SIZE, "%c%hu", perm, dom);
    req[1].len = strlen(value) + 1;
    rep = xenbus_msg_reply(XS_SET_PERMS, xbt, req, ARRAY_SIZE(req));
    msg = errmsg(rep);
    if (msg)
	return msg;
    free(rep);
    return NULL;
}

char *xenbus_transaction_start(xenbus_transaction_t *xbt)
{
    /* xenstored becomes angry if you send a length 0 message, so just
       shove a nul terminator on the end */
    struct write_req req = { "", 1};
    struct xsd_sockmsg *rep;
    char *err;

    rep = xenbus_msg_reply(XS_TRANSACTION_START, 0, &req, 1);
    err = errmsg(rep);
    if (err)
	return err;

    /* hint: typeof(*xbt) == unsigned long */
    *xbt = strtoul((char *)(rep+1), NULL, 10);

    free(rep);
    return NULL;
}

char *
xenbus_transaction_end(xenbus_transaction_t t, int abort, int *retry)
{
    struct xsd_sockmsg *rep;
    struct write_req req;
    char *err;

    *retry = 0;

    req.data = abort ? "F" : "T";
    req.len = 2;
    rep = xenbus_msg_reply(XS_TRANSACTION_END, t, &req, 1);
    err = errmsg(rep);
    if (err) {
	if (!strcmp(err, "EAGAIN")) {
	    *retry = 1;
	    free(err);
	    return NULL;
	} else {
	    return err;
	}
    }
    free(rep);
    return NULL;
}

int xenbus_read_integer(const char *path)
{
    char *res, *buf;
    int t;

    res = xenbus_read(XBT_NIL, path, &buf);
    if (res) {
	printk("Failed to read %s.\n", path);
	free(res);
	return -1;
    }
    t = strtoul(buf, NULL, 10);
    free(buf);
    return t;
}

char* xenbus_printf(xenbus_transaction_t xbt,
                                  const char* node, const char* path,
                                  const char* fmt, ...)
{
#define BUFFER_SIZE 256
    char fullpath[BUFFER_SIZE];
    char val[BUFFER_SIZE];
    va_list args;

    BUG_ON(strlen(node) + strlen(path) + 1 >= BUFFER_SIZE);
    sprintf(fullpath,"%s/%s", node, path);
    va_start(args, fmt);
    vsprintf(val, fmt, args);
    va_end(args);
    return xenbus_write(xbt,fullpath,val);
}

domid_t xenbus_get_self_id(void)
{
    char *dom_id;
    domid_t ret;

    BUG_ON(xenbus_read(XBT_NIL, "domid", &dom_id));
    ret = strtoul(dom_id, NULL, 10);

    return ret;
}

static void do_ls_test(const char *pre)
{
    char **dirs, *msg;
    int x;

    printk("ls %s...\n", pre);
    msg = xenbus_ls(XBT_NIL, pre, &dirs);
    if (msg) {
	printk("Error in xenbus ls: %s\n", msg);
	free(msg);
	return;
    }
    for (x = 0; dirs[x]; x++) 
    {
        printk("ls %s[%d] -> %s\n", pre, x, dirs[x]);
        free(dirs[x]);
    }
    free(dirs);
}

static void do_read_test(const char *path)
{
    char *res, *msg;
    printk("Read %s...\n", path);
    msg = xenbus_read(XBT_NIL, path, &res);
    if (msg) {
	printk("Error in xenbus read: %s\n", msg);
	free(msg);
	return;
    }
    printk("Read %s -> %s.\n", path, res);
    free(res);
}

static void do_write_test(const char *path, const char *val)
{
    char *msg;
    printk("Write %s to %s...\n", val, path);
    msg = xenbus_write(XBT_NIL, path, val);
    if (msg) {
	printk("Result %s\n", msg);
	free(msg);
    } else {
	printk("Success.\n");
    }
}

static void do_rm_test(const char *path)
{
    char *msg;
    printk("rm %s...\n", path);
    msg = xenbus_rm(XBT_NIL, path);
    if (msg) {
	printk("Result %s\n", msg);
	free(msg);
    } else {
	printk("Success.\n");
    }
}

/* Simple testing thing */
void test_xenbus(void)
{
    printk("Doing xenbus test.\n");
    xenbus_debug_msg("Testing xenbus...\n");

    printk("Doing ls test.\n");
    do_ls_test("device");
    do_ls_test("device/vif");
    do_ls_test("device/vif/0");

    printk("Doing read test.\n");
    do_read_test("device/vif/0/mac");
    do_read_test("device/vif/0/backend");

    printk("Doing write test.\n");
    do_write_test("device/vif/0/flibble", "flobble");
    do_read_test("device/vif/0/flibble");
    do_write_test("device/vif/0/flibble", "widget");
    do_read_test("device/vif/0/flibble");

    printk("Doing rm test.\n");
    do_rm_test("device/vif/0/flibble");
    do_read_test("device/vif/0/flibble");
    printk("(Should have said ENOENT)\n");
}

/*
 * Local variables:
 * mode: C
 * c-basic-offset: 4
 * End:
 */
