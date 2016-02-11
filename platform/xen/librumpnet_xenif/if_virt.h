/*	$NetBSD: if_virt.h,v 1.2 2013/07/04 11:58:11 pooka Exp $	*/

/*
 * NOTE!  This file is supposed to work on !NetBSD platforms.
 */

#ifndef VIRTIF_BASE
#error Define VIRTIF_BASE
#endif

#define VIF_STRING(x) #x
#define VIF_STRINGIFY(x) VIF_STRING(x)
#define VIF_CONCAT(x,y) x##y
#define VIF_CONCAT3(x,y,z) x##y##z
#define VIF_BASENAME(x,y) VIF_CONCAT(x,y)
#define VIF_BASENAME3(x,y,z) VIF_CONCAT3(x,y,z)

#define VIF_CLONER VIF_BASENAME(VIRTIF_BASE,_cloner)
#define VIF_NAME VIF_STRINGIFY(VIRTIF_BASE)

#define VIFHYPER_CREATE VIF_BASENAME3(rumpcomp_,VIRTIF_BASE,_create)
#define VIFHYPER_DYING VIF_BASENAME3(rumpcomp_,VIRTIF_BASE,_dying)
#define VIFHYPER_DESTROY VIF_BASENAME3(rumpcomp_,VIRTIF_BASE,_destroy)
#define VIFHYPER_SEND VIF_BASENAME3(rumpcomp_,VIRTIF_BASE,_send)

struct virtif_sc;
void rump_virtif_pktdeliver(struct virtif_sc *, struct iovec *, size_t);
