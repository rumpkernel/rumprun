/*
 * xenbus_dev.c
 * 
 * Driver giving user-space access to the kernel's xenbus connection
 * to xenstore.  Adapted heavily from NetBSD's xenbus_dev.c, so much
 * so that practically none of the original remains.
 * 
 * Copyright (c) 2014 Citrix
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
 *
 * (From original xenbus_dev.c:)
 * Copyright (c) 2005, Christian Limpach
 * Copyright (c) 2005, Rusty Russell, IBM Corporation
 *
 * This file may be distributed separately from the Linux kernel, or
 * incorporated into other software packages, subject to the following license:
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: $");

#include "rumpxen_xendev.h"

#include <bmk-rumpuser/rumpuser.h>

#define BUFFER_SIZE (XENSTORE_PAYLOAD_MAX+sizeof(struct xsd_sockmsg))

#include <xen/io/xs_wire.h>

#include <mini-os/xenbus.h>
#include <mini-os/wait.h>

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

struct xenbus_dev_data {
	kmutex_t lock;
	int outstanding_requests;
	LIST_HEAD(, xenbus_dev_transaction) transactions;
	LIST_HEAD(, xenbus_dev_watch) watches;
	struct xenbus_event_queue replies; /* Entirely unread by user. */

	_Bool queued_enomem, want_restart;

	/* Partially written request(s). */
	unsigned int wbuf_used;
	union {
		struct xsd_sockmsg msg;
		unsigned char buffer[BUFFER_SIZE];
	} wbuf;

	/* Partially read response. */
	struct xsd_sockmsg *rmsg; /* .id==user_id; data follows */
	int rmsg_done;
	void (*rmsg_free)(void*);

	struct selinfo selinfo;
	/* The lock used for the purposes described in select(9)
	 * is xenbus_req_lock, not d->lock. */
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
find_transaction(struct xenbus_dev_data *d, xenbus_transaction_t id)
{
	struct xenbus_dev_transaction *trans;

	LIST_FOREACH(trans, &d->transactions, entry)
		if (trans->tx_id == d->wbuf.msg.tx_id)
			return trans;
	/* not found */
	return 0;
}

static struct xenbus_dev_watch*
find_visible_watch(struct xenbus_dev_data *d,
		   const char *path, const char *token)
{
	struct xenbus_dev_watch *watch;

	LIST_FOREACH(watch, &d->watches, entry)
		if (watch->visible_to_user &&
		    !strcmp(path, watch->path) &&
		    !strcmp(token, watch->user_token))
			return watch;
	/* not found */
	return 0;
}

/*----- request handling (writes to the device) -----*/

static void
make_request(struct xenbus_dev_data *d, struct xenbus_dev_request *req,
	     uint32_t tx_id, const struct write_req *wreqs, int num_wreqs)
/* Caller should have filled in req->req_id, ->u, and (if needed)
 * ->user_id.  We deal with ->xb and ->xb_id. */
{
	req->xb.watch = 0;
	req->xb_id = xenbus_id_allocate(&d->replies, &req->xb);

	KASSERT(d->outstanding_requests < INT_MAX);
	d->outstanding_requests++;

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
make_watch_request(struct xenbus_dev_data *d, struct xenbus_dev_request *req,
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
write_trouble(struct xenbus_dev_data *d, const char *what)
{
	printf("xenbus dev: bad write: %s\n", what);

#ifdef RUMP_DEV_XEN_DEBUG
	{
		unsigned int i;
		printf(" %d bytes:", d->wbuf_used);
		for (i=0; i<d->wbuf_used; i++) {
			if (!(i & 3)) printf(" ");
			printf("%02x", d->wbuf.buffer[i]);
		}
		printf(".\n");
	}
#endif /*RUMP_DEV_XEN_DEBUG*/

	d->wbuf_used = 0; /* discard everything buffered */
}

/* void __NORETURN__ WTROUBLE(const char *details_without_newline);
 * assumes:   struct xenbus_dev_data *d;
 *            int err;
 *            end: */
#define WTROUBLE(s) do{ write_trouble(d,s); err = EINVAL; goto end; }while(0)

static void
forward_request(struct xenbus_dev_data *d, struct xenbus_dev_request *req)
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

static int
process_request(struct xenbus_dev_data *d)
{
	struct xenbus_dev_request *req;
	struct xenbus_dev_transaction *trans;
	struct xenbus_dev_watch *watch_free = 0, *watch;
	const char *wpath, *wtoken;
	int err;

	DPRINTF(("/dev/xen/xenbus: request, type=%d\n",
		 d->wbuf.msg.type));

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
				WTROUBLE("unknown transaction");
		}
		forward_request(d, req);
		break;

	case XS_TRANSACTION_START:
		if (d->wbuf.msg.tx_id)
			WTROUBLE("nested transaction");
		req->u.trans = xbd_malloc(sizeof(*req->u.trans));
		if (!req->u.trans) {
			err = ENOMEM;
			goto end;
		}
		forward_request(d, req);
		break;

	case XS_TRANSACTION_END:
		if (!d->wbuf.msg.tx_id)
			WTROUBLE("ending zero transaction");
		req->u.trans = trans = find_transaction(d, d->wbuf.msg.tx_id);
		if (!trans)
			WTROUBLE("ending unknown transaction");
		LIST_REMOVE(trans, entry); /* prevent more reqs using it */
		forward_request(d, req);
		break;
 
	case XS_WATCH:
		if (d->wbuf.msg.tx_id)
			WTROUBLE("XS_WATCH with transaction");
		if (!watch_message_parse(&d->wbuf.msg, &wpath, &wtoken))
			WTROUBLE("bad XS_WATCH message");

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

		watch->xb.events = &d->replies;
		xenbus_watch_prepare(&watch->xb);

		watch_free = 0; /* we are committed */
		watch->visible_to_user = 0;
		LIST_INSERT_HEAD(&d->watches, watch, entry);
		make_watch_request(d, req, d->wbuf.msg.tx_id, watch);
		break;

	case XS_UNWATCH:
		if (d->wbuf.msg.tx_id)
			WTROUBLE("XS_UNWATCH with transaction");
		if (!watch_message_parse(&d->wbuf.msg, &wpath, &wtoken))
			WTROUBLE("bad XS_WATCH message");

		watch = find_visible_watch(d, wpath, wtoken);
		if (!watch)
			WTROUBLE("unwatch nonexistent watch");

		watch->visible_to_user = 0;
		make_watch_request(d, req, d->wbuf.msg.tx_id, watch);
		break;

	default:
		WTROUBLE("unknown request message type");
	}

	err = 0;
end:
	if (watch_free)
		free_watch(watch_free);
	return err;
}

static int
xenbus_dev_write(struct file *fp, off_t *offset, struct uio *uio,
		 kauth_cred_t cred, int flags)
{
	struct xenbus_dev_data *d = fp->f_data;
	int err;

	DPRINTF(("/dev/xen/xenbus: write...\n"));

	if (uio->uio_offset < 0)
		return EINVAL;

	mutex_enter(&d->lock);

	for (;;) { /* keep reading more until we're done */

		if (!uio->uio_resid)
			break;

		uio->uio_offset = d->wbuf_used;
		err = uiomove(d->wbuf.buffer + d->wbuf_used,
			      sizeof(d->wbuf.buffer) -  d->wbuf_used,
			      uio);
		d->wbuf_used = uio->uio_offset;
		if (err)
			goto end;

		for (;;) { /* process message(s) in the buffer */

			if (d->wbuf_used < sizeof(d->wbuf.msg))
				break;

			if (d->wbuf.msg.len > XENSTORE_PAYLOAD_MAX)
				WTROUBLE("too much payload in packet");

			uint32_t packetlen =
				d->wbuf.msg.len + sizeof(d->wbuf.msg);

			KASSERT(packetlen <= sizeof(d->wbuf.buffer));

			if (d->wbuf_used < packetlen)
				break;

			err = process_request(d);

			if (d->wbuf_used) {
				/* Remove from the buffer before checking
				 * for errors - but some errors may have
				 * emptied the buffer already. */
				d->wbuf_used -= packetlen;
				memmove(d->wbuf.buffer,
					d->wbuf.buffer + packetlen,
					d->wbuf_used);
			}

			if (err)
				goto end;
		}
	}

	err = 0;
end:
	mutex_exit(&d->lock);

	DPRINTF(("/dev/xen/xenbus: write done, err=%d\n", err));
	return err;
}

/*----- response and watch event handling (reads from the device) -----*/

static struct xsd_sockmsg*
process_watch_event(struct xenbus_dev_data *d, struct xenbus_event *event,
		    struct xenbus_dev_watch *watch,
		    void (**mfree_r)(void*))
{

	/* We need to make a new XS_WATCH_EVENT message because the
	 * one from xenstored (a) isn't visible to us here and (b)
	 * anyway has the wrong token in it. */

	DPRINTF(("/dev/xen/xenbus: watch event,"
		 " wpath=%s user_token=%s epath=%s xb.token=%s\n",
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
process_response(struct xenbus_dev_data *d, struct xenbus_dev_request *req,
		 void (**mfree_r)(void*))
{
	struct xenbus_dev_watch *watch;
	struct xsd_sockmsg *msg = req->xb.reply;

	msg->req_id = req->user_id;

	_Bool error = msg->type == XS_ERROR;
	KASSERT(error || msg->type == req->req_type);

	DPRINTF(("/dev/xen/xenbus: response, req_type=%d msg->type=%d\n",
		 req->req_type, msg->type));

	switch (req->req_type) {

	case XS_TRANSACTION_START:
		if (error)
			break;
		KASSERT(msg->len >= 2);
		KASSERT(!((uint8_t*)(msg+1))[msg->len-1]);
		req->u.trans->tx_id =
			strtoul((char*)&msg + sizeof(*msg),
				0, 0);
		LIST_INSERT_HEAD(&d->transactions, req->u.trans,
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
	KASSERT(d->outstanding_requests > 0);
	d->outstanding_requests--;

	*mfree_r = xenbus_free;
	return msg;
}

static struct xsd_sockmsg*
process_event(struct xenbus_dev_data *d, struct xenbus_event *event,
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

static struct xsd_sockmsg*
next_event_msg(struct xenbus_dev_data *d, struct file *fp, int *err_r,
	       void (**mfree_r)(void*))
/* If !err_r, always blocks and always returns successfully.
 * If !!err_r, will block iff user process read should block:
 * will either return successfully, or set *err_r and return 0.
 *
 * Must be called with d->lock held; may temporarily release it. */
{
	int nlocks;
	DEFINE_WAIT(w);
	spin_lock(&xenbus_req_lock);

	while (STAILQ_EMPTY(&d->replies.events)) {
		if (err_r) {
			if (d->want_restart) {
				*err_r = ERESTART;
				goto fail;
			}
			if (fp->f_flag & FNONBLOCK) {
				*err_r = EAGAIN;
				goto fail;
			}
		}

		DPRINTF(("/dev/xen/xenbus: about to block err_r=%p\n", err_r));

		minios_add_waiter(w, d->replies.waitq);
		spin_unlock(&xenbus_req_lock);
		mutex_exit(&d->lock);
		rumpkern_unsched(&nlocks, 0);

		minios_wait(w);

		rumpkern_sched(nlocks, 0);
		mutex_enter(&d->lock);
		spin_lock(&xenbus_req_lock);
		minios_remove_waiter(w, d->replies.waitq);
	}
	struct xenbus_event *event = STAILQ_FIRST(&d->replies.events);
	STAILQ_REMOVE_HEAD(&d->replies.events, entry);

	spin_unlock(&xenbus_req_lock);

	DPRINTF(("/dev/xen/xenbus: next_event_msg found an event %p\n",event));
	return process_event(d, event, mfree_r);

fail:
	DPRINTF(("/dev/xen/xenbus: not blocking, returning %d\n", *err_r));
	spin_unlock(&xenbus_req_lock);
	return 0;
}

static int
xenbus_dev_read(struct file *fp, off_t *offset, struct uio *uio,
		kauth_cred_t cred, int flags)
{
	struct xenbus_dev_data *d = fp->f_data;
	size_t org_resid = uio->uio_resid;
	int err;

	DPRINTF(("/dev/xen/xenbus: read...\n"));
	mutex_enter(&d->lock);

	for (;;) {
		DPRINTF(("/dev/xen/xenbus: read... uio_resid=%lu (org=%lu)"
			 " q.enomem=%d\n",
			 (unsigned long)uio->uio_resid,
			 (unsigned long)org_resid,
			 d->queued_enomem));
		if (d->queued_enomem) {
			if (org_resid != uio->uio_resid)
				/* return early now; report it next time */
				break;
			err = ENOMEM;
			d->queued_enomem = 0;
			goto end;
		}

		if (!uio->uio_resid)
			/* done what we have been asked to do */
			break;

		if (!d->rmsg) {
			d->rmsg = next_event_msg(d, fp, &err, &d->rmsg_free);
			if (!d->rmsg) {
				if (uio->uio_resid != org_resid)
					/* Done something, claim success. */
					break;
				goto end;
			}
		}

		uint32_t avail = sizeof(*d->rmsg) + d->rmsg->len;
		KASSERT(avail < BUFFER_SIZE*2); /* sanity check */
		KASSERT(avail > 0);
		KASSERT(d->rmsg_done <= avail);

		DPRINTF(("/dev/xen/xenbus: read... rmsg->len=%lu"
			 " msg_done=%lu avail=%lu\n",
			 (unsigned long)d->rmsg->len,
			 (unsigned long)d->rmsg_done,
			 (unsigned long)avail));

		uio->uio_offset = d->rmsg_done;
		err = uiomove((char*)d->rmsg + d->rmsg_done,
			      avail - d->rmsg_done,
			      uio);
		d->rmsg_done = uio->uio_offset;
		if (err)
			goto end;

		if (d->rmsg_done == avail) {
			DPRINTF(("/dev/xen/xenbus: read... msg complete\n"));
			d->rmsg_free(d->rmsg);
			d->rmsg = 0;
			d->rmsg_done = 0;
		}
	}

	err = 0;

end:
	mutex_exit(&d->lock);
	DPRINTF(("/dev/xen/xenbus: read done, err=%d\n", err));
	return err;
}

/*----- more exciting reading -----*/

#define RBITS (POLLIN  | POLLRDNORM)
#define WBITS (POLLOUT | POLLWRNORM)

static void
xenbus_dev_xb_wakeup(struct xenbus_event_queue *queue)
{
	/* called with req_lock held */
	DPRINTF(("/dev/xen/xenbus: wakeup\n"));
	struct xenbus_dev_data *d =
		container_of(queue, struct xenbus_dev_data, replies);
	minios_wake_up(&d->replies.waitq);
	selnotify(&d->selinfo, RBITS, NOTE_SUBMIT);
}

static void
xenbus_dev_restart(file_t *fp)
{
	struct xenbus_dev_data *d = fp->f_data;

	DPRINTF(("/dev/xen/xenbus: restart!\n"));

	mutex_enter(&d->lock);
	spin_lock(&xenbus_req_lock);

	d->want_restart |= 1;
	minios_wake_up(&d->replies.waitq);

	spin_unlock(&xenbus_req_lock);
	mutex_exit(&d->lock);
}

static int
xenbus_dev_poll(struct file *fp, int events)
{
	struct xenbus_dev_data *d = fp->f_data;
	int revents = 0;

	DPRINTF(("/dev/xen/xenbus: poll events=0%o...\n", events));

	mutex_enter(&d->lock);
	spin_lock(&xenbus_req_lock);

	/* always writeable - we don't do proper blocking for writing
	 * since this can only wait at most until other requests have
	 * been handled by xenstored */
	revents |= events & WBITS;

	if (events & RBITS)
		if (d->rmsg || d->queued_enomem || d->want_restart)
			revents |= events & RBITS;

	if (!revents) {
		if (events & RBITS)
			selrecord(curlwp, &d->selinfo);
	}

	spin_unlock(&xenbus_req_lock);
	mutex_exit(&d->lock);

	DPRINTF(("/dev/xen/xenbus: poll events=0%o done, revents=0%o\n",
		 events, revents));
	return revents;
}

/*----- setup etc. -----*/

static int
xenbus_dev_close(struct file *fp)
{
	struct xenbus_dev_data *d = fp->f_data;

	DPRINTF(("/dev/xen/xenbus: close...\n"));

	/* Not neeeded against concurrent access (we assume!)
	 * but next_event_msg will want to unlock and relock it */
	mutex_enter(&d->lock);

	xbd_free(d->rmsg);
	d->rmsg = 0;

	for (;;) {
		DPRINTF(("/dev/xen/xenbus: close loop\n"));
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
				make_watch_request(d, &watch->destroy, 0,
						   watch);
			}
		}

		struct xenbus_dev_transaction *trans, *trans_tmp;
		const struct write_req trans_end_data = { "F", 2 };
		LIST_FOREACH_SAFE(trans, &d->transactions, entry, trans_tmp) {
			DPRINTF(("/dev/xen/xenbus: close transaction"
				 " %p %"PRIx32"\n",
				 trans, (unsigned int)trans->tx_id));
			/* mirrors process_request XS_TRANSACTION_END */
			trans->destroy.req_type = XS_TRANSACTION_END;
			trans->destroy.u.trans = trans;
			LIST_REMOVE(trans, entry);
			make_request(d, &trans->destroy, trans->tx_id,
				     &trans_end_data, 1);
		}

		DPRINTF(("/dev/xen/xenbus: close outstanding=%d\n",
			 d->outstanding_requests));
		KASSERT(d->outstanding_requests >= 0);
		if (!d->outstanding_requests)
			break;

		void (*dfree)(void*);
		struct xsd_sockmsg *discard = next_event_msg(d, fp, 0, &dfree);
		KASSERT(discard);
		dfree(discard);
	}

	KASSERT(!d->outstanding_requests);
	KASSERT(!d->rmsg);
	KASSERT(LIST_EMPTY(&d->transactions));
	KASSERT(LIST_EMPTY(&d->watches));

	DPRINTF(("/dev/xen/xenbus: close seldestroy outstanding=%d\n",
                d->outstanding_requests));
	seldestroy(&d->selinfo);
	xbd_free(d);

	DPRINTF(("/dev/xen/xenbus: close done.\n"));
	return 0;
}

const struct fileops xenbus_dev_fileops = {
        .fo_read = xenbus_dev_read,
        .fo_write = xenbus_dev_write,
        .fo_ioctl = fbadop_ioctl,
        .fo_fcntl = fnullop_fcntl,
        .fo_poll = xenbus_dev_poll,
        .fo_stat = fbadop_stat,
        .fo_close = xenbus_dev_close,
        .fo_kqfilter = fnullop_kqfilter,
        .fo_restart = xenbus_dev_restart,
};

int
xenbus_dev_open(struct file *fp, void **fdata_r)
{
	struct xenbus_dev_data *d;

	d = xbd_malloc(sizeof(*d));
	if (!d)
		return ENOMEM;

	mutex_init(&d->lock, MUTEX_DEFAULT, IPL_HIGH);
	d->outstanding_requests = 0;
	LIST_INIT(&d->transactions);
	LIST_INIT(&d->watches);
	xenbus_event_queue_init(&d->replies);
	d->replies.wakeup = xenbus_dev_xb_wakeup;
	d->queued_enomem = 0;
	d->want_restart = 0;
	d->wbuf_used = 0;
	d->rmsg = 0;
	d->rmsg_done = 0;
	selinit(&d->selinfo);

	*fdata_r = d;
	return 0;
}

/*
 * Local variables:
 *  c-file-style: "linux"
 *  indent-tabs-mode: t
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
