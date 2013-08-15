/* -*-  Mode:C; c-basic-offset:4; tab-width:4 -*-
 ****************************************************************************
 * (C) 2003 - Rolf Neugebauer - Intel Research Cambridge
 ****************************************************************************
 *
 *        File: lib.h
 *      Author: Rolf Neugebauer (neugebar@dcs.gla.ac.uk)
 *     Changes: 
 *              
 *        Date: Aug 2003
 * 
 * Environment: Xen Minimal OS
 * Description: Random useful library functions, contains some freebsd stuff
 *
 ****************************************************************************
 * $Id: h-insert.h,v 1.4 2002/11/08 16:03:55 rn Exp $
 ****************************************************************************
 *
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _LIB_H_
#define _LIB_H_

#include <stdarg.h>
#include <stddef.h>
#include <xen/xen.h>
#include <xen/event_channel.h>
#include <sys/queue.h>
#include "gntmap.h"

#ifdef HAVE_LIBC
#include <stdio.h>
#else
#include <lib-gpl.h>
#endif

#ifdef HAVE_LIBC
#include <string.h>
#else
/* string and memory manipulation */

/*
 * From:
 *	@(#)libkern.h	8.1 (Berkeley) 6/10/93
 * $FreeBSD$
 */
int	 memcmp(const void *b1, const void *b2, size_t len);

char	*strcat(char * __restrict, const char * __restrict);
int	 strcmp(const char *, const char *);
char	*strcpy(char * __restrict, const char * __restrict);

char	*strdup(const char *__restrict);

size_t	 strlen(const char *);

int	 strncmp(const char *, const char *, size_t);
char	*strncpy(char * __restrict, const char * __restrict, size_t);

char	*strstr(const char *, const char *);

void *memset(void *, int, size_t);

char *strchr(const char *p, int ch);
char *strrchr(const char *p, int ch);

/* From:
 *	@(#)systm.h	8.7 (Berkeley) 3/29/95
 * $FreeBSD$
 */
void	*memcpy(void *to, const void *from, size_t len);

size_t strnlen(const char *, size_t);
#endif

#include <mini-os/console.h>

#define RAND_MIX 2654435769U

int rand(void);

#include <mini-os/xenbus.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define ASSERT(x)                                              \
do {                                                           \
	if (!(x)) {                                                \
		printk("ASSERTION FAILED: %s at %s:%d.\n",             \
			   # x ,                                           \
			   __FILE__,                                       \
			   __LINE__);                                      \
        BUG();                                                 \
	}                                                          \
} while(0)

#define BUG_ON(x) ASSERT(!(x))

/* Consistency check as much as possible. */
void sanity_check(void);

#ifdef HAVE_LIBC
enum fd_type {
    FTYPE_NONE = 0,
    FTYPE_CONSOLE,
    FTYPE_FILE,
    FTYPE_XENBUS,
    FTYPE_XC,
    FTYPE_EVTCHN,
    FTYPE_GNTMAP,
    FTYPE_SOCKET,
    FTYPE_TAP,
    FTYPE_BLK,
    FTYPE_KBD,
    FTYPE_FB,
    FTYPE_MEM,
    FTYPE_SAVEFILE,
};

LIST_HEAD(evtchn_port_list, evtchn_port_info);

struct evtchn_port_info {
        LIST_ENTRY(evtchn_port_info) list;
        evtchn_port_t port;
        unsigned long pending;
        int bound;
};

extern struct file {
    enum fd_type type;
    union {
	struct {
            /* lwIP fd */
	    int fd;
	} socket;
	struct {
            /* FS import fd */
	    int fd;
	    off_t offset;
	} file;
	struct {
	    struct evtchn_port_list ports;
	} evtchn;
	struct gntmap gntmap;
	struct {
	    struct netfront_dev *dev;
	} tap;
	struct {
	    struct blkfront_dev *dev;
	} blk;
	struct {
	    struct kbdfront_dev *dev;
	} kbd;
	struct {
	    struct fbfront_dev *dev;
	} fb;
	struct {
	    struct consfront_dev *dev;
	} cons;
#ifdef CONFIG_XENBUS
        struct {
            /* To each xenbus FD is associated a queue of watch events for this
             * FD.  */
            xenbus_event_queue events;
        } xenbus;
#endif
    };
    int read;	/* maybe available for read */
} files[];

int alloc_fd(enum fd_type type);
void close_all_files(void);
extern struct thread *main_thread;
void sparse(unsigned long data, size_t size);
#endif

#endif /* _LIB_H_ */
