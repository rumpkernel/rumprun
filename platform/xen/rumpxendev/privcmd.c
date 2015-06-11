/* $NetBSD: privcmd.c,v 1.49 2014/10/17 16:37:02 christos Exp $ */

/*-
 * Copyright (c) 2004 Christian Limpach.
 * Copyright (c) 2015 Wei Liu.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: privcmd.c,v 1.49 2014/10/17 16:37:02 christos Exp $");

#include <sys/errno.h>
#include <sys/mount.h>
#include <sys/dirent.h>
#include <uvm/uvm_prot.h>
#include <sys/vnode_if.h>
#include <sys/vnode.h>
#include <miscfs/kernfs/kernfs.h>

#include "rumpxen_xendev.h"

#include <mini-os/mm.h>

#include "xenio.h"

static int
xenprivcmd_xen2bsd_errno(int error)
{
	/*
	 * Xen uses System V error codes.
	 * In order to keep bloat as minimal as possible,
	 * only convert what really impact us.
	 */

	switch(-error) {
	case 0:
		return 0;
	case 1:
		return EPERM;
	case 2:
		return ENOENT;
	case 3:
		return ESRCH;
	case 4:
		return EINTR;
	case 5:
		return EIO;
	case 6:
		return ENXIO;
	case 7:
		return E2BIG;
	case 8:
		return ENOEXEC;
	case 9:
		return EBADF;
	case 10:
		return ECHILD;
	case 11:
		return EAGAIN;
	case 12:
		return ENOMEM;
	case 13:
		return EACCES;
	case 14:
		return EFAULT;
	case 15:
		return ENOTBLK;
	case 16:
		return EBUSY;
	case 17:
		return EEXIST;
	case 18:
		return EXDEV;
	case 19:
		return ENODEV;
	case 20:
		return ENOTDIR;
	case 21:
		return EISDIR;
	case 22:
		return EINVAL;
	case 23:
		return ENFILE;
	case 24:
		return EMFILE;
	case 25:
		return ENOTTY;
	case 26:
		return ETXTBSY;
	case 27:
		return EFBIG;
	case 28:
		return ENOSPC;
	case 29:
		return ESPIPE;
	case 30:
		return EROFS;
	case 31:
		return EMLINK;
	case 32:
		return EPIPE;
	case 33:
		return EDOM;
	case 34:
		return ERANGE;
	case 35:
		return EDEADLK;
	case 36:
		return ENAMETOOLONG;
	case 37:
		return ENOLCK;
	case 38:
		return ENOSYS;
	case 39:
		return ENOTEMPTY;
	case 40:
		return ELOOP;
	case 42:
		return ENOMSG;
	case 43:
		return EIDRM;
	case 60:
		return ENOSTR;
	case 61:
		return ENODATA;
	case 62:
		return ETIME;
	case 63:
		return ENOSR;
	case 66:
		return EREMOTE;
	case 74:
		return EBADMSG;
	case 75:
		return EOVERFLOW;
	case 84:
		return EILSEQ;
	case 87:
		return EUSERS;
	case 88:
		return ENOTSOCK;
	case 89:
		return EDESTADDRREQ;
	case 90:
		return EMSGSIZE;
	case 91:
		return EPROTOTYPE;
	case 92:
		return ENOPROTOOPT;
	case 93:
		return EPROTONOSUPPORT;
	case 94:
		return ESOCKTNOSUPPORT;
	case 95:
		return EOPNOTSUPP;
	case 96:
		return EPFNOSUPPORT;
	case 97:
		return EAFNOSUPPORT;
	case 98:
		return EADDRINUSE;
	case 99:
		return EADDRNOTAVAIL;
	case 100:
		return ENETDOWN;
	case 101:
		return ENETUNREACH;
	case 102:
		return ENETRESET;
	case 103:
		return ECONNABORTED;
	case 104:
		return ECONNRESET;
	case 105:
		return ENOBUFS;
	case 106:
		return EISCONN;
	case 107:
		return ENOTCONN;
	case 108:
		return ESHUTDOWN;
	case 109:
		return ETOOMANYREFS;
	case 110:
		return ETIMEDOUT;
	case 111:
		return ECONNREFUSED;
	case 112:
		return EHOSTDOWN;
	case 113:
		return EHOSTUNREACH;
	case 114:
		return EALREADY;
	case 115:
		return EINPROGRESS;
	case 116:
		return ESTALE;
	case 122:
		return EDQUOT;
	default:
		printf("unknown xen error code %d\n", -error);
		return -error;
	}
}

static int
xenprivcmd_ioctl(void *v)
{
	int err;
	struct vop_ioctl_args *ap = v;

	switch (ap->a_command) {
	case IOCTL_PRIVCMD_HYPERCALL:
	{
		privcmd_hypercall_t *hc = (privcmd_hypercall_t *)ap->a_data;

		err = minios_hypercall(hc->op, hc->arg[0], hc->arg[1],
				       hc->arg[2], hc->arg[3], hc->arg[4]);
		if (err >= 0) {
			hc->retval = err;
			err = 0;
		} else {
			err = xenprivcmd_xen2bsd_errno(err);
			hc->retval = 0;
		}

		break;
	}
	case IOCTL_PRIVCMD_MMAP:
	{
		int i;
		privcmd_mmap_t *mcmd = ap->a_data;
		privcmd_mmap_entry_t mentry;

		for (i = 0; i < mcmd->num; i++) {
			err = copyin(&mcmd->entry[i], &mentry, sizeof(mentry));
			if (err)
				return err;

			if (mentry.npages == 0 || mentry.va & PAGE_MASK)
				return EINVAL;

			/* Call with err argument == NULL will just crash
			 * the domain.
			 */
			minios_map_frames(mentry.va, &mentry.mfn, mentry.npages,
					  0, 0, mcmd->dom, NULL,
					  minios_get_l1prot());
		}

		err = 0;
		break;
	}
	case IOCTL_PRIVCMD_MMAPBATCH:
	{
		privcmd_mmapbatch_t *pmb = ap->a_data;

		if (pmb->num == 0 || pmb->addr & PAGE_MASK)
			return EINVAL;

		/* Call with err argument == NULL will just crash the
		 * domain.
		 */
		minios_map_frames(pmb->addr, pmb->arr, pmb->num, 1, 0,
				  pmb->dom, NULL, minios_get_l1prot());
		err = 0;
		break;
	}
	default:
		err = EINVAL;
	}

	return err;
}

static const struct kernfs_fileop xenprivcmd_fileops[] = {
	{ .kf_fileop = KERNFS_FILEOP_IOCTL, .kf_vop = xenprivcmd_ioctl },
};

#define XENPRIVCMD_MODE (S_IRUSR)
extern kernfs_parentdir_t *kernxen_pkt;
void xenprivcmd_init(void)
{
	kernfs_entry_t *dkt;
	kfstype kfst;

	kfst = KERNFS_ALLOCTYPE(xenprivcmd_fileops);

	KERNFS_ALLOCENTRY(dkt, M_TEMP, M_WAITOK);
	KERNFS_INITENTRY(dkt, DT_REG, "privcmd", NULL, kfst, VREG,
			 XENPRIVCMD_MODE);
	kernfs_addentry(kernxen_pkt, dkt);
}
