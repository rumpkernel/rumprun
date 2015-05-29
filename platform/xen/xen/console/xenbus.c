#include <mini-os/types.h>
#include <mini-os/wait.h>
#include <mini-os/mm.h>
#include <mini-os/hypervisor.h>
#include <mini-os/events.h>
#include <mini-os/os.h>
#include <mini-os/lib.h>
#include <mini-os/xenbus.h>
#include <xen/io/console.h>
#include <xen/io/protocols.h>
#include <xen/io/ring.h>
#include <mini-os/gnttab.h>

#include "console.h"

#include <bmk-core/memalloc.h>
#include <bmk-core/printf.h>
#include <bmk-core/string.h>

void free_consfront(struct consfront_dev *dev)
{
    char* err = NULL;
    XenbusState state;

    char path[bmk_strlen(dev->backend) + 1 + 5 + 1];
    char nodename[bmk_strlen(dev->nodename) + 1 + 5 + 1];

    bmk_snprintf(path, sizeof(path), "%s/state", dev->backend);
    bmk_snprintf(nodename, sizeof(nodename), "%s/state", dev->nodename);

    if ((err = xenbus_switch_state(XBT_NIL, nodename, XenbusStateClosing)) != NULL) {
        minios_printk("free_consfront: error changing state to %d: %s\n",
                XenbusStateClosing, err);
        goto close;
    }
    state = xenbus_read_integer(path);
    while (err == NULL && state < XenbusStateClosing)
        err = xenbus_wait_for_state_change(path, &state, &dev->events);
    if (err) bmk_memfree(err);

    if ((err = xenbus_switch_state(XBT_NIL, nodename, XenbusStateClosed)) != NULL) {
        minios_printk("free_consfront: error changing state to %d: %s\n",
                XenbusStateClosed, err);
        goto close;
    }

close:
    if (err) bmk_memfree(err);
    xenbus_unwatch_path_token(XBT_NIL, path, path);

    minios_mask_evtchn(dev->evtchn);
    minios_unbind_evtchn(dev->evtchn);
    bmk_memfree(dev->backend);
    bmk_memfree(dev->nodename);

    gnttab_end_access(dev->ring_ref);

    minios_free_page(dev->ring);
    bmk_memfree(dev);
}

struct consfront_dev *init_consfront(char *_nodename)
{
    xenbus_transaction_t xbt;
    char* err;
    char* message=NULL;
    int retry=0;
    char* msg = NULL;
    char path[64];
    static int consfrontends = 3;
    struct consfront_dev *dev;
    int res;

    dev = bmk_memcalloc(1, sizeof(*dev));

    if (!_nodename)
        bmk_snprintf(dev->nodename, sizeof(dev->nodename),
	  "device/console/%d", consfrontends);
    else
        bmk_strncpy(dev->nodename, _nodename, sizeof(dev->nodename)-1);
    consfrontends++;

    minios_printk("******************* CONSFRONT for %s **********\n\n\n", dev->nodename);

    bmk_snprintf(path, sizeof(path), "%s/backend-id", dev->nodename);
    if ((res = xenbus_read_integer(path)) < 0) 
        return NULL;
    else
        dev->dom = res;
    minios_evtchn_alloc_unbound(dev->dom, console_handle_input, dev, &dev->evtchn);

    dev->ring = (struct xencons_interface *) minios_alloc_page();
    bmk_memset(dev->ring, 0, PAGE_SIZE);
    dev->ring_ref = gnttab_grant_access(dev->dom, virt_to_mfn(dev->ring), 0);

    xenbus_event_queue_init(&dev->events);

again:
    err = xenbus_transaction_start(&xbt);
    if (err) {
        minios_printk("starting transaction\n");
        bmk_memfree(err);
    }

    err = xenbus_printf(xbt, dev->nodename, "ring-ref","%u",
                dev->ring_ref);
    if (err) {
        message = "writing ring-ref";
        goto abort_transaction;
    }
    err = xenbus_printf(xbt, dev->nodename,
                "port", "%u", dev->evtchn);
    if (err) {
        message = "writing event-channel";
        goto abort_transaction;
    }
    err = xenbus_printf(xbt, dev->nodename,
                "protocol", "%s", XEN_IO_PROTO_ABI_NATIVE);
    if (err) {
        message = "writing protocol";
        goto abort_transaction;
    }

    err = xenbus_printf(xbt, dev->nodename, "type", "%s", "ioemu");
    if (err) {
        message = "writing type";
        goto abort_transaction;
    }

    bmk_snprintf(path, sizeof(path), "%s/state", dev->nodename);
    err = xenbus_switch_state(xbt, path, XenbusStateConnected);
    if (err) {
        message = "switching state";
        goto abort_transaction;
    }


    err = xenbus_transaction_end(xbt, 0, &retry);
    if (err) bmk_memfree(err);
    if (retry) {
            goto again;
        minios_printk("completing transaction\n");
    }

    goto done;

abort_transaction:
    bmk_memfree(err);
    err = xenbus_transaction_end(xbt, 1, &retry);
    minios_printk("Abort transaction %s\n", message);
    goto error;

done:

    bmk_snprintf(path, sizeof(path), "%s/backend", dev->nodename);
    msg = xenbus_read(XBT_NIL, path, &dev->backend);
    if (msg) {
        minios_printk("Error %s when reading the backend path %s\n", msg, path);
        goto error;
    }

    minios_printk("backend at %s\n", dev->backend);

    {
        XenbusState state;
        char path[bmk_strlen(dev->backend) + 1 + 19 + 1];
        bmk_snprintf(path, sizeof(path), "%s/state", dev->backend);
        
	xenbus_watch_path_token(XBT_NIL, path, path, &dev->events);
        msg = NULL;
        state = xenbus_read_integer(path);
        while (msg == NULL && state < XenbusStateConnected)
            msg = xenbus_wait_for_state_change(path, &state, &dev->events);
        if (msg != NULL || state != XenbusStateConnected) {
            minios_printk("backend not available, state=%d\n", state);
            xenbus_unwatch_path_token(XBT_NIL, path, path);
            goto error;
        }
    }
    minios_unmask_evtchn(dev->evtchn);

    minios_printk("**************************\n");

    return dev;

error:
    bmk_memfree(msg);
    bmk_memfree(err);
    free_consfront(dev);
    return NULL;
}

void fini_console(struct consfront_dev *dev)
{
    if (dev) free_consfront(dev);
}

