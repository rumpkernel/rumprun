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


int		sprintf(char *, const char *, ...);
int		snprintf(char *, size_t, const char *, ...);
void		vprintf(const char *, va_list);
int		vsprintf(char *, const char *, va_list);
int		vsnprintf(char *, size_t, const char *, va_list);
unsigned long	strtoul(const char *, char **, int);

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


#endif /* _LIB_H_ */
