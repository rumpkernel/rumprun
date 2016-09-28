
#include <mini-os/os.h>
#include <mini-os/xenbus.h>
#include <mini-os/wait.h>

#include <bmk-core/memalloc.h>
#include <bmk-core/string.h>
#include <bmk-core/errno.h>

#include <bmk-rumpuser/core_types.h>
#include <bmk-rumpuser/rumpuser.h>

#include "busdev_user.h"

#define xbd_malloc(sz) (bmk_memalloc((sz), 0, BMK_MEMWHO_RUMPKERN))

static inline void xbd_free(void *p) { bmk_memfree(p, BMK_MEMWHO_RUMPKERN); }

#define strcmp bmk_strcmp
#define strlen bmk_strlen
#define memchr bmk_memchr
#define memcpy bmk_memcpy
#define strtoul bmk_strtoul

#define bzero(p,l) bmk_memset((p),0,(l))

#define KASSERT bmk_assert
#define assert  bmk_assert
#define INT_MAX ((int)(~(unsigned int)0 >> 1u))

#define ENOMEM BMK_ENOMEM
#define EINVAL BMK_EINVAL

#define printf minios_printk

#ifdef RUMP_DEV_XEN_DEBUG
#define DPRINTF(a) (printf a)
#else
#define DPRINTF(a) /* nothing */
#endif

static char *xbd_strdup(const char *s) {
	size_t l = strlen(s);
	char *r = xbd_malloc(l + 1);
	if (!r) return r;
	memcpy(r, s, l+1);
	return r;
}

/*----- data structures -----*/

struct xenbus_dev_request {
	struct xenbus_event xb;
	uint32_t xb_id, user_id;
	uint32_t req_type;
	union {
		struct xenbus_dev_transaction *trans;
		struct xenbus_dev_watch *watch;
	} u;
};

struct xenbus_dev_transaction {
	LIST_ENTRY(xenbus_dev_transaction) entry;
	xenbus_transaction_t tx_id;
	struct xenbus_dev_request destroy;
};

struct xenbus_dev_watch {
	struct xenbus_watch xb;
	LIST_ENTRY(xenbus_dev_watch) entry;
	struct xsd_sockmsg *wmsg;
	char *path, *user_token;
	_Bool visible_to_user;
	struct xenbus_dev_request destroy;
};

struct rumpxenbus_data_user {
	struct rumpxenbus_data_common *c;
	int outstanding_requests;
	LIST_HEAD(, xenbus_dev_transaction) transactions;
	LIST_HEAD(, xenbus_dev_watch) watches;
	struct xenbus_event_queue replies; /* Entirely unread by user. */
};

/*----- helpers -----*/

static void
free_watch(struct xenbus_dev_watch *watch)
{
	xbd_free(watch->path);
	xbd_free(watch->user_token);
	xbd_free(watch);
}

static struct xenbus_dev_transaction*
find_transaction(struct rumpxenbus_data_common *d, xenbus_transaction_t id)
{
	struct rumpxenbus_data_user *const du = d->du;
	struct xenbus_dev_transaction *trans;

	LIST_FOREACH(trans, &du->transactions, entry)
		if (trans->tx_id == d->wbuf.msg.tx_id)
			return trans;
	/* not found */
	return 0;
}

static struct xenbus_dev_watch*
find_visible_watch(struct rumpxenbus_data_common *d,
		   const char *path, const char *token)
{
	struct rumpxenbus_data_user *const du = d->du;
	struct xenbus_dev_watch *watch;

	LIST_FOREACH(watch, &du->watches, entry)
		if (watch->visible_to_user &&
		    !strcmp(path, watch->path) &&
		    !strcmp(token, watch->user_token))
			return watch;
	/* not found */
	return 0;
}

/*----- request handling (writes to the device) -----*/

static void
make_request(struct rumpxenbus_data_common *d, struct xenbus_dev_request *req,
	     uint32_t tx_id, const struct write_req *wreqs, int num_wreqs)
/* Caller should have filled in req->req_id, ->u, and (if needed)
 * ->user_id.  We deal with ->xb and ->xb_id. */
{
	struct rumpxenbus_data_user *const du = d->du;

	req->xb.watch = 0;
	req->xb_id = xenbus_id_allocate(&du->replies, &req->xb);

	KASSERT(du->outstanding_requests < INT_MAX);
	du->outstanding_requests++;

	xenbus_xb_write(req->req_type, req->xb_id, tx_id,
			wreqs, num_wreqs);
}

static void
watch_write_req_string(struct write_req **wreqp, const char *string)
{
	struct write_req *wreq = (*wreqp)++;
	int l = strlen(string);
	wreq->len = l+1;
	wreq->data = string;
}

static void
make_watch_request(struct rumpxenbus_data_common *d,
		   struct xenbus_dev_request *req,
		   uint32_t tx_id, struct xenbus_dev_watch *watch)
{
	struct write_req wreqs[2], *wreq = wreqs;
	watch_write_req_string(&wreq, watch->path);
	watch_write_req_string(&wreq, watch->xb.token);
	KASSERT((char*)wreq == (char*)wreqs + sizeof(wreqs));

	req->u.watch = watch;
	make_request(d, req, tx_id, wreqs, 2);
}

static void
forward_request(struct rumpxenbus_data_common *d, struct xenbus_dev_request *req)
{
	struct write_req wreq = {
		d->wbuf.buffer + sizeof(d->wbuf.msg),
		d->wbuf_used - sizeof(d->wbuf.msg),
	};

	make_request(d, req, d->wbuf.msg.tx_id, &wreq, 1);
}

static _Bool
watch_message_parse_string(const char **p, const char *end,
			   const char **string_r)
{
	const char *nul = memchr(*p, 0, end - *p);
	if (!nul)
		return 0;

	*string_r = *p;
	*p = nul+1;

	return 1;
}

static _Bool
watch_message_parse(const struct xsd_sockmsg *msg,
		    const char **path_r, const char **token_r)
{
	const char *begin = (const char*)msg;
	const char *p = begin + sizeof(*msg);
	const char *end = p + msg->len;
	KASSERT(p <= end);

	return
		watch_message_parse_string(&p, end, path_r) &&
		watch_message_parse_string(&p, end, token_r);
}

int
rumpxenbus_process_request(struct rumpxenbus_data_common *d)
{
	struct rumpxenbus_data_user *const du = d->du;
	struct xenbus_dev_request *req;
	struct xenbus_dev_transaction *trans;
	struct xenbus_dev_watch *watch_free = 0, *watch;
	const char *wpath, *wtoken;
	int err;

	DPRINTF(("/dev/xen/xenbus[%p,du=%p]: request, type=%d\n",
		 d,du, d->wbuf.msg.type));

	req = xbd_malloc(sizeof(*req));
	if (!req) {
		err = ENOMEM;
		goto end;
	}
	req->user_id = d->wbuf.msg.req_id;
	req->req_type = d->wbuf.msg.type;

	switch (d->wbuf.msg.type) {
	case XS_DIRECTORY:
	case XS_READ:
	case XS_GET_PERMS:
	case XS_GET_DOMAIN_PATH:
	case XS_IS_DOMAIN_INTRODUCED:
	case XS_WRITE:
	case XS_MKDIR:
	case XS_RM:
	case XS_SET_PERMS:
		if (d->wbuf.msg.tx_id) {
			if (!find_transaction(d, d->wbuf.msg.tx_id))
				WTROUBLE(d,"unknown transaction");
		}
		forward_request(d, req);
		break;

	case XS_TRANSACTION_START:
		if (d->wbuf.msg.tx_id)
			WTROUBLE(d,"nested transaction");
		req->u.trans = xbd_malloc(sizeof(*req->u.trans));
		if (!req->u.trans) {
			err = ENOMEM;
			goto end;
		}
		forward_request(d, req);
		break;

	case XS_TRANSACTION_END:
		if (!d->wbuf.msg.tx_id)
			WTROUBLE(d,"ending zero transaction");
		req->u.trans = trans = find_transaction(d, d->wbuf.msg.tx_id);
		if (!trans)
			WTROUBLE(d,"ending unknown transaction");
		LIST_REMOVE(trans, entry); /* prevent more reqs using it */
		forward_request(d, req);
		break;
 
	case XS_WATCH:
		if (d->wbuf.msg.tx_id)
			WTROUBLE(d,"XS_WATCH with transaction");
		if (!watch_message_parse(&d->wbuf.msg, &wpath, &wtoken))
			WTROUBLE(d,"bad XS_WATCH message");

		watch = watch_free = xbd_malloc(sizeof(*watch));
		if (!watch) {
			err = ENOMEM;
			goto end;
		}

		watch->path = xbd_strdup(wpath);
		watch->user_token = xbd_strdup(wtoken);
		if (!watch->path || !watch->user_token) {
			err = ENOMEM;
			goto end;
		}

		watch->xb.events = &du->replies;
		xenbus_watch_prepare(&watch->xb);

		watch_free = 0; /* we are committed */
		watch->visible_to_user = 0;
		LIST_INSERT_HEAD(&du->watches, watch, entry);
		make_watch_request(d, req, d->wbuf.msg.tx_id, watch);
		break;

	case XS_UNWATCH:
		if (d->wbuf.msg.tx_id)
			WTROUBLE(d,"XS_UNWATCH with transaction");
		if (!watch_message_parse(&d->wbuf.msg, &wpath, &wtoken))
			WTROUBLE(d,"bad XS_WATCH message");

		watch = find_visible_watch(d, wpath, wtoken);
		if (!watch)
			WTROUBLE(d,"unwatch nonexistent watch");

		watch->visible_to_user = 0;
		make_watch_request(d, req, d->wbuf.msg.tx_id, watch);
		break;

	default:
		WTROUBLE(d,"unknown request message type");
	}

	err = 0;
end:
	if (watch_free)
		free_watch(watch_free);
	return err;
}

/*----- response and watch event handling (reads from the device) -----*/

static struct xsd_sockmsg*
process_watch_event(struct rumpxenbus_data_common *d, struct xenbus_event *event,
		    struct xenbus_dev_watch *watch,
		    void (**mfree_r)(void*))
{

	/* We need to make a new XS_WATCH_EVENT message because the
	 * one from xenstored (a) isn't visible to us here and (b)
	 * anyway has the wrong token in it. */

	DPRINTF(("/dev/xen/xenbus[%p]: watch event,"
		 " wpath=%s user_token=%s epath=%s xb.token=%s\n",
                 d,
		 watch->path, watch->user_token,
		 event->path, watch->xb.token));

	/* Define the parts of the message */

#define WATCH_MESSAGE_PART_STRING(PART,x)		\
	PART(strlen((x)) + 1, memcpy(p, (x), sz))

#define WATCH_MESSAGE_PARTS(PART)				\
	PART(sizeof(struct xsd_sockmsg), (void)0)		\
	WATCH_MESSAGE_PART_STRING(PART,event->path)		\
	WATCH_MESSAGE_PART_STRING(PART,watch->user_token)

	/* Compute the size */

	size_t totalsz = 0;
	size_t sz = 0;

#define WATCH_MESSAGE_PART_ADD_SIZE(calcpartsz, fill) \
	totalsz += (calcpartsz);

	WATCH_MESSAGE_PARTS(WATCH_MESSAGE_PART_ADD_SIZE);

	DPRINTF(("/dev/xen/xenbus: watch event allocating %lu\n",
		 (unsigned long)totalsz));

	/* Allocate it and fill in the header */

	struct xsd_sockmsg *reply = xbd_malloc(totalsz);
	if (!reply) {
		printf("xenbus dev: out of memory for watch event"
		       " wpath=`%s' epath=`%s'\n",
		       watch->path, event->path);
		d->queued_enomem = 1;
		goto end;
	}

	bzero(reply, sizeof(*reply));
	reply->type = XS_WATCH_EVENT;
	reply->len = totalsz - sizeof(*reply);

	char *p = (void*)reply;

	/* Fill in the rest of the message */

#define WATCH_MESSAGE_PART_ADD(calcpartsz, fill)	\
	sz = (calcpartsz);				\
	fill;						\
	p += sz;

	WATCH_MESSAGE_PARTS(WATCH_MESSAGE_PART_ADD);

	KASSERT(p == (const char*)reply + totalsz);

	/* Now we are done */

end:
	xenbus_free(event);
	*mfree_r = xbd_free;
	return reply;
}

/* Returned value is from malloc() */
static struct xsd_sockmsg*
process_response(struct rumpxenbus_data_common *d, struct xenbus_dev_request *req,
		 void (**mfree_r)(void*))
{
	struct rumpxenbus_data_user *const du = d->du;
	struct xenbus_dev_watch *watch;
	struct xsd_sockmsg *msg = req->xb.reply;

	msg->req_id = req->user_id;

	_Bool error = msg->type == XS_ERROR;
	KASSERT(error || msg->type == req->req_type);

	DPRINTF(("/dev/xen/xenbus[%p,du=%p]:"
                 " response, req_type=%d msg->type=%d\n",
		 d,du, req->req_type, msg->type));

	switch (req->req_type) {

	case XS_TRANSACTION_START:
		if (error)
			break;
		KASSERT(msg->len >= 2);
		KASSERT(!((uint8_t*)(msg+1))[msg->len-1]);
		req->u.trans->tx_id =
			strtoul((char*)&msg + sizeof(*msg),
				0, 0);
		LIST_INSERT_HEAD(&du->transactions, req->u.trans,
				 entry);
		break;

	case XS_TRANSACTION_END:
		xbd_free(req->u.trans);
		break;

	case XS_WATCH:
		watch = req->u.watch;
		if (error)
			goto do_unwatch;
		watch->visible_to_user = 1;
		break;

	case XS_UNWATCH:
		KASSERT(!error);
		watch = req->u.watch;
	do_unwatch:
		KASSERT(!watch->visible_to_user);
		LIST_REMOVE(watch, entry);
		xenbus_watch_release(&watch->xb);
		free_watch(watch);
		break;

	}

	xenbus_id_release(req->xb_id);
	xbd_free(req);
	KASSERT(du->outstanding_requests > 0);
	du->outstanding_requests--;

	*mfree_r = xenbus_free;
	return msg;
}

static struct xsd_sockmsg*
process_event(struct rumpxenbus_data_common *d, struct xenbus_event *event,
	      void (**mfree_r)(void*))
{
	if (event->watch) {
		struct xenbus_dev_watch *watch =
			container_of(event->watch, struct xenbus_dev_watch, xb);

		return process_watch_event(d, event, watch, mfree_r);

	} else {
		struct xenbus_dev_request *req =
			container_of(event, struct xenbus_dev_request, xb);

		return process_response(d, req, mfree_r);
	}

}

struct xsd_sockmsg*
rumpxenbus_next_event_msg(struct rumpxenbus_data_common *dc,
			 _Bool block,
			 void (**mfree_r)(void*))
/* If !!block, always blocks and always returns successfully.
 * If !block, stores err_r_if_nothing into *err_r rather than blocking.

 * If !!err_r, will block iff user process read should block:
 * will either return successfully, or set *err_r and return 0.
 *
 * Must be called with d->lock held; may temporarily release it
 * by calling rumpxenbus_block_{before,after}. */
{
	struct rumpxenbus_data_user *d = dc->du;
	int nlocks;
	DEFINE_WAIT(w);
	spin_lock(&xenbus_req_lock);

	while (STAILQ_EMPTY(&d->replies.events)) {
		if (!block)
			goto fail;

		DPRINTF(("/dev/xen/xenbus[%p,du=%p]: about to block\n",dc,d));

		minios_add_waiter(w, d->replies.waitq);
		spin_unlock(&xenbus_req_lock);
		rumpxenbus_block_before(dc);
		rumpkern_unsched(&nlocks, 0);

		minios_wait(w);

		rumpkern_sched(nlocks, 0);
		rumpxenbus_block_after(dc);
		spin_lock(&xenbus_req_lock);
		minios_remove_waiter(w, d->replies.waitq);
	}
	struct xenbus_event *event = STAILQ_FIRST(&d->replies.events);
	STAILQ_REMOVE_HEAD(&d->replies.events, entry);

	spin_unlock(&xenbus_req_lock);

	DPRINTF(("/dev/xen/xenbus: next_event_msg found an event %p\n",event));
	return process_event(dc, event, mfree_r);

fail:
	DPRINTF(("/dev/xen/xenbus: not blocking, returning no event\n"));
	spin_unlock(&xenbus_req_lock);
	return 0;
}

/*----- more exciting reading -----*/

static void
xenbus_dev_xb_wakeup(struct xenbus_event_queue *queue)
{
	/* called with req_lock held */
	struct rumpxenbus_data_user *d =
		container_of(queue, struct rumpxenbus_data_user, replies);
	DPRINTF(("/dev/xen/xenbus[queue=%p,du=%p]: wakeup...\n",queue,d));
	minios_wake_up(&d->replies.waitq);
	rumpxenbus_dev_xb_wakeup(d->c);
}

void
rumpxenbus_dev_restart_wakeup(struct rumpxenbus_data_common *c)
{
	struct rumpxenbus_data_user *d = c->du;
	spin_lock(&xenbus_req_lock);
	minios_wake_up(&d->replies.waitq);
	spin_unlock(&xenbus_req_lock);
}

void
rumpxenbus_dev_user_shutdown(struct rumpxenbus_data_common *dc)
{
	struct rumpxenbus_data_user *d = dc->du;
	for (;;) {
		DPRINTF(("/dev/xen/xenbus[%p,du=%p]: close loop\n",dc,d));
		/* We need to go round this again and again because
		 * there might be requests in flight.  Eg if the
		 * user has an XS_WATCH in flight we have to wait for it
		 * to be done and then unwatch it again. */

		struct xenbus_dev_watch *watch, *watch_tmp;
		LIST_FOREACH_SAFE(watch, &d->watches, entry, watch_tmp) {
			DPRINTF(("/dev/xen/xenbus: close watch %p %d\n",
				 watch, watch->visible_to_user));
			if (watch->visible_to_user) {
				/* mirrors process_request XS_UNWATCH */
				watch->destroy.req_type = XS_UNWATCH;
				watch->visible_to_user = 0;
				make_watch_request(dc, &watch->destroy, 0,
						   watch);
			}
		}

		struct xenbus_dev_transaction *trans, *trans_tmp;
		const struct write_req trans_end_data = { "F", 2 };
		LIST_FOREACH_SAFE(trans, &d->transactions, entry, trans_tmp) {
			DPRINTF(("/dev/xen/xenbus: close transaction"
				 " %p %lx\n",
				 trans, (unsigned long)trans->tx_id));
			/* mirrors process_request XS_TRANSACTION_END */
			trans->destroy.req_type = XS_TRANSACTION_END;
			trans->destroy.u.trans = trans;
			LIST_REMOVE(trans, entry);
			make_request(dc, &trans->destroy, trans->tx_id,
				     &trans_end_data, 1);
		}

		DPRINTF(("/dev/xen/xenbus: close outstanding=%d\n",
			 d->outstanding_requests));
		KASSERT(d->outstanding_requests >= 0);
		if (!d->outstanding_requests)
			break;

		void (*dfree)(void*);

		struct xsd_sockmsg *discard =
			rumpxenbus_next_event_msg(dc, 1, &dfree);

		KASSERT(discard);
		dfree(discard);
	}

	KASSERT(!d->outstanding_requests);
	KASSERT(LIST_EMPTY(&d->transactions));
	KASSERT(LIST_EMPTY(&d->watches));

	xbd_free(d);
	dc->du = NULL;
}

int
rumpxenbus_dev_user_open(struct rumpxenbus_data_common *dc)
{
	assert(!dc->du);

	struct rumpxenbus_data_user *d = dc->du = xbd_malloc(sizeof(*dc->du));
	if (!d)
		return ENOMEM;

	DPRINTF(("/dev/xen/xenbus[%p,dd=%p]: open: user...\n",dc,d));

	d->c = dc;
	d->outstanding_requests = 0;
	LIST_INIT(&d->transactions);
	LIST_INIT(&d->watches);
	xenbus_event_queue_init(&d->replies);
	d->replies.wakeup = xenbus_dev_xb_wakeup;

	dc->wbuf_used = 0;
	dc->queued_enomem = 0;

	return 0;
}
