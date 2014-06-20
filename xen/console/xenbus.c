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
#include <mini-os/xmalloc.h>
#include <mini-os/gnttab.h>

#include "console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void free_consfront(struct consfront_dev *dev)
{
    char* err = NULL;
    XenbusState state;

    char path[strlen(dev->backend) + 1 + 5 + 1];
    char nodename[strlen(dev->nodename) + 1 + 5 + 1];

    snprintf(path, sizeof(path), "%s/state", dev->backend);
    snprintf(nodename, sizeof(nodename), "%s/state", dev->nodename);

    if ((err = xenbus_switch_state(XBT_NIL, nodename, XenbusStateClosing)) != NULL) {
        printk("free_consfront: error changing state to %d: %s\n",
                XenbusStateClosing, err);
        goto close;
    }
    state = xenbus_read_integer(path);
    while (err == NULL && state < XenbusStateClosing)
        err = xenbus_wait_for_state_change(path, &state, &dev->events);
    if (err) free(err);

    if ((err = xenbus_switch_state(XBT_NIL, nodename, XenbusStateClosed)) != NULL) {
        printk("free_consfront: error changing state to %d: %s\n",
                XenbusStateClosed, err);
        goto close;
    }

close:
    if (err) free(err);
    xenbus_unwatch_path_token(XBT_NIL, path, path);

    mask_evtchn(dev->evtchn);
    unbind_evtchn(dev->evtchn);
    free(dev->backend);
    free(dev->nodename);

    gnttab_end_access(dev->ring_ref);

    free_page(dev->ring);
    free(dev);
}

struct consfront_dev *init_consfront(char *_nodename)
{
    xenbus_transaction_t xbt;
    char* err;
    char* message=NULL;
    int retry=0;
    char* msg = NULL;
    char nodename[256];
    char path[256];
    static int consfrontends = 3;
    struct consfront_dev *dev;
    int res;

    if (!_nodename)
        snprintf(nodename, sizeof(nodename), "device/console/%d", consfrontends);
    else
        strncpy(nodename, _nodename, sizeof(nodename));

    printk("******************* CONSFRONT for %s **********\n\n\n", nodename);

    consfrontends++;
    dev = malloc(sizeof(*dev));
    memset(dev, 0, sizeof(*dev));
    dev->nodename = strdup(nodename);

    snprintf(path, sizeof(path), "%s/backend-id", nodename);
    if ((res = xenbus_read_integer(path)) < 0) 
        return NULL;
    else
        dev->dom = res;
    evtchn_alloc_unbound(dev->dom, console_handle_input, dev, &dev->evtchn);

    dev->ring = (struct xencons_interface *) alloc_page();
    memset(dev->ring, 0, PAGE_SIZE);
    dev->ring_ref = gnttab_grant_access(dev->dom, virt_to_mfn(dev->ring), 0);

    xenbus_event_queue_init(&dev->events);

again:
    err = xenbus_transaction_start(&xbt);
    if (err) {
        printk("starting transaction\n");
        free(err);
    }

    err = xenbus_printf(xbt, nodename, "ring-ref","%u",
                dev->ring_ref);
    if (err) {
        message = "writing ring-ref";
        goto abort_transaction;
    }
    err = xenbus_printf(xbt, nodename,
                "port", "%u", dev->evtchn);
    if (err) {
        message = "writing event-channel";
        goto abort_transaction;
    }
    err = xenbus_printf(xbt, nodename,
                "protocol", "%s", XEN_IO_PROTO_ABI_NATIVE);
    if (err) {
        message = "writing protocol";
        goto abort_transaction;
    }

    err = xenbus_printf(xbt, nodename, "type", "%s", "ioemu");
    if (err) {
        message = "writing type";
        goto abort_transaction;
    }

    snprintf(path, sizeof(path), "%s/state", nodename);
    err = xenbus_switch_state(xbt, path, XenbusStateConnected);
    if (err) {
        message = "switching state";
        goto abort_transaction;
    }


    err = xenbus_transaction_end(xbt, 0, &retry);
    if (err) free(err);
    if (retry) {
            goto again;
        printk("completing transaction\n");
    }

    goto done;

abort_transaction:
    free(err);
    err = xenbus_transaction_end(xbt, 1, &retry);
    printk("Abort transaction %s\n", message);
    goto error;

done:

    snprintf(path, sizeof(path), "%s/backend", nodename);
    msg = xenbus_read(XBT_NIL, path, &dev->backend);
    if (msg) {
        printk("Error %s when reading the backend path %s\n", msg, path);
        goto error;
    }

    printk("backend at %s\n", dev->backend);

    {
        XenbusState state;
        char path[strlen(dev->backend) + 1 + 19 + 1];
        snprintf(path, sizeof(path), "%s/state", dev->backend);
        
	xenbus_watch_path_token(XBT_NIL, path, path, &dev->events);
        msg = NULL;
        state = xenbus_read_integer(path);
        while (msg == NULL && state < XenbusStateConnected)
            msg = xenbus_wait_for_state_change(path, &state, &dev->events);
        if (msg != NULL || state != XenbusStateConnected) {
            printk("backend not available, state=%d\n", state);
            xenbus_unwatch_path_token(XBT_NIL, path, path);
            goto error;
        }
    }
    unmask_evtchn(dev->evtchn);

    printk("**************************\n");

    return dev;

error:
    free(msg);
    free(err);
    free_consfront(dev);
    return NULL;
}

void fini_console(struct consfront_dev *dev)
{
    if (dev) free_consfront(dev);
}

