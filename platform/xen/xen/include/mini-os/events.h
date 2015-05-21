/* -*-  Mode:C; c-basic-offset:4; tab-width:4 -*-
 ****************************************************************************
 * (C) 2003 - Rolf Neugebauer - Intel Research Cambridge
 * (C) 2005 - Grzegorz Milos - Intel Reseach Cambridge
 ****************************************************************************
 *
 *        File: events.h
 *      Author: Rolf Neugebauer (neugebar@dcs.gla.ac.uk)
 *     Changes: Grzegorz Milos (gm281@cam.ac.uk)
 *              
 *        Date: Jul 2003, changes Jun 2005
 * 
 * Environment: Xen Minimal OS
 * Description: Deals with events on the event channels
 *
 ****************************************************************************
 */

#ifndef _MINIOS_EVENTS_H_
#define _MINIOS_EVENTS_H_

#include<xen/event_channel.h>

struct pt_regs;
typedef void (*evtchn_handler_t)(evtchn_port_t, struct pt_regs *, void *);

/* prototypes */
int do_event(evtchn_port_t port, struct pt_regs *regs);
evtchn_port_t minios_bind_virq(uint32_t virq, evtchn_handler_t handler, void *data);
evtchn_port_t minios_bind_pirq(uint32_t pirq, int will_share, evtchn_handler_t handler, void *data);
evtchn_port_t minios_bind_evtchn(evtchn_port_t port, evtchn_handler_t handler,
						  void *data);
void minios_unbind_evtchn(evtchn_port_t port);
void init_events(void);
int minios_evtchn_alloc_unbound(domid_t pal, evtchn_handler_t handler,
						 void *data, evtchn_port_t *port);
int minios_evtchn_bind_interdomain(domid_t pal, evtchn_port_t remote_port,
							evtchn_handler_t handler, void *data,
							evtchn_port_t *local_port);
void unbind_all_ports(void);

int minios_notify_remote_via_evtchn(evtchn_port_t port);

int minios_event_channel_op(int cmd, void *op);

void fini_events(void);

#endif /* _MINIOS_EVENTS_H_ */
