/* -*-  Mode:C; c-basic-offset:4; tab-width:4 -*-
 ****************************************************************************
 * (C) 2003 - Rolf Neugebauer - Intel Research Cambridge
 ****************************************************************************
 *
 *        File: types.h
 *      Author: Rolf Neugebauer (neugebar@dcs.gla.ac.uk)
 *     Changes: 
 *              
 *        Date: May 2003
 * 
 * Environment: Xen Minimal OS
 * Description: a random collection of type definitions
 *
 ****************************************************************************
 * $Id: h-insert.h,v 1.4 2002/11/08 16:03:55 rn Exp $
 ****************************************************************************
 */

#ifndef _MINIOS_TYPES_H_
#define _MINIOS_TYPES_H_

/* XXX: fix the kernelside Xen driver #include abuse */
#ifndef __RUMP_KERNEL__
#include <bmk-core/types.h>

#ifndef _BSD_SIZE_T_
typedef unsigned int		u_int;
typedef unsigned long		u_long;
typedef unsigned long		size_t;
#endif

#ifndef offsetof
#define offsetof(_t_,_e_) __builtin_offsetof(_t_,_e_)
#endif
#endif

#ifdef __i386__
typedef struct { unsigned long pte_low, pte_high; } pte_t;

#elif defined(__x86_64__)

typedef struct { unsigned long pte; } pte_t;
#endif /* __i386__ || __x86_64__ */

#ifdef __x86_64__
#define __pte(x) ((pte_t) { (x) } )
#else
#define __pte(x) ({ unsigned long long _x = (x);        \
    ((pte_t) {(unsigned long)(_x), (unsigned long)(_x>>32)}); })
#endif

#endif /* _MINIOS_TYPES_H_ */
