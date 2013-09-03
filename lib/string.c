/* -*-  Mode:C; c-basic-offset:4; tab-width:4 -*-
 ****************************************************************************
 * (C) 2003 - Rolf Neugebauer - Intel Research Cambridge
 ****************************************************************************
 *
 *        File: string.c
 *      Author: Rolf Neugebauer (neugebar@dcs.gla.ac.uk)
 *     Changes: 
 *              
 *        Date: Aug 2003
 * 
 * Environment: Xen Minimal OS
 * Description: Library function for string and memory manipulation
 *              Origin unknown
 *
 ****************************************************************************
 * $Id: c-insert.c,v 1.7 2002/11/08 16:04:34 rn Exp $
 ****************************************************************************
 */

#include <sys/types.h>
#include <sys/null.h>

#include <mini-os/xmalloc.h>

#include <string.h>

char *strdup(const char *x)
{
    int l = strlen(x);
    char *res = malloc(l + 1);
	if (!res) return NULL;
    memcpy(res, x, l + 1);
    return res;
}
