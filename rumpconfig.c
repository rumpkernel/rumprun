/*
 * Copyright (c) 2014 Martin Lucina.  All Rights Reserved.
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
 */

#include <err.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rump/rump.h>
#include <rump/rump_syscalls.h>
#include <rump/netconfig.h>

#include <mini-os/os.h>
#include <mini-os/mm.h>
#include <mini-os/xenbus.h>

#include <ufs/ufs/ufsmount.h>
#include <isofs/cd9660/cd9660_mount.h>

#include "rumpconfig.h"

static int
xs_read_netconfig(const char *if_index, char **type, char **method, char **addr,
		char **mask, char **gw)
{
	char *if_type = NULL;
	char *if_method = NULL;
	char *if_addr = NULL;
	char *if_mask = NULL;
	char *if_gw = NULL;
	char buf[128];
	char *xberr = NULL;
	xenbus_transaction_t txn;
	int xbretry = 0;

	xberr = xenbus_transaction_start(&txn);
	if (xberr) {
		warnx("rumprun_config: xenbus_transaction_start() failed: %s",
			xberr);
		return 1;
	}
	snprintf(buf, sizeof buf, "rumprun/net/%s/type", if_index);
	xberr = xenbus_read(txn, buf, &if_type);
	if (xberr) {
		warnx("rumprun_config: xenif%s: read %s failed: %s",
			if_index, buf, xberr);
		xenbus_transaction_end(txn, 0, &xbretry);
		return 1;
	}
	snprintf(buf, sizeof buf, "rumprun/net/%s/method", if_index);
	xberr = xenbus_read(txn, buf, &if_method);
	if (xberr) {
		warnx("rumprun_config: xenif%s: read %s failed: %s",
			if_index, buf, xberr);
		xenbus_transaction_end(txn, 0, &xbretry);
		free(if_type);
		return 1;
	}
	/* The following parameters are dependent on the type/method. */
	snprintf(buf, sizeof buf, "rumprun/net/%s/addr", if_index);
	xberr = xenbus_read(txn, buf, &if_addr);
	if (xberr && strcmp(xberr, "ENOENT") != 0) {
		warnx("rumprun_config: xenif%s: read %s failed: %s",
			if_index, buf, xberr);
		xenbus_transaction_end(txn, 0, &xbretry);
		free(if_type);
		return 1;
	}
	snprintf(buf, sizeof buf, "rumprun/net/%s/netmask", if_index);
	xberr = xenbus_read(txn, buf, &if_mask);
	if (xberr && strcmp(xberr, "ENOENT") != 0) {
		warnx("rumprun_config: xenif%s: read %s failed: %s",
			if_index, buf, xberr);
		xenbus_transaction_end(txn, 0, &xbretry);
		free(if_type);
		return 1;
	}
	snprintf(buf, sizeof buf, "rumprun/net/%s/gw", if_index);
	xberr = xenbus_read(txn, buf, &if_gw);
	if (xberr && strcmp(xberr, "ENOENT") != 0) {
		warnx("rumprun_config: xenif%s: read %s failed: %s",
			if_index, buf, xberr);
		xenbus_transaction_end(txn, 0, &xbretry);
		free(if_type);
		return 1;
	}
	xberr = xenbus_transaction_end(txn, 0, &xbretry);
	if (xberr) {
		warnx("rumprun_config: xenbus_transaction_end() failed: %s",
			xberr);
		free(if_type);
		free(if_method);
		return 1;
	}
	*type = if_type;
	*method = if_method;
	*addr = if_addr;
	*mask = if_mask;
	*gw = if_gw;
	return 0;
}

static void
rumprun_config_net(const char *if_index)
{
	char *if_type = NULL;
	char *if_method = NULL;
	char *if_addr = NULL;
	char *if_mask = NULL;
	char *if_gw = NULL;
	char buf[128];
	int rv;
	
	rv = xs_read_netconfig(if_index, &if_type, &if_method, &if_addr,
		&if_mask, &if_gw);
	if (rv != 0)
		return;
	
	printf("rumprun_config: configuring xenif%s as %s with %s %s\n",
		if_index, if_type, if_method, if_addr ? if_addr : "");
	snprintf(buf, sizeof buf, "xenif%s", if_index);
	if ((rv = rump_pub_netconfig_ifcreate(buf)) != 0) {
		warnx("rumprun_config: %s: ifcreate failed: %s\n", buf,
			strerror(rv));
		goto out;
	}
	if (strcmp(if_type, "inet") == 0 &&
	    strcmp(if_method, "dhcp") == 0) {
		if ((rv = rump_pub_netconfig_dhcp_ipv4_oneshot(buf)) != 0) {
			warnx("rumprun_config: %s: dhcp_ipv4 failed: %s\n", buf,
				strerror(rv));
			goto out;
		}
	}
	else if (strcmp(if_type, "inet") == 0 &&
		 strcmp(if_method, "static") == 0) {
		if (if_addr == NULL || if_mask == NULL) {
			warnx("rumprun_config: %s: missing if_addr/mask\n");
			goto out;
		}
		if ((rv = rump_pub_netconfig_ipv4_ifaddr(buf, if_addr,
			if_mask)) != 0) {
			warnx("rumprun_config: %s: ipv4_ifaddr failed: %s\n",
				buf, strerror(rv));
			goto out;
		}
		if (if_gw &&
			(rv = rump_pub_netconfig_ipv4_gw(if_gw)) != 0) {
			warnx("rumprun_config: %s: ipv4_gw failed: %s\n",
				buf, strerror(rv));
			goto out;
		}
	}
	else {
		warnx("rumprun_config: %s: unknown type/method %s/%s\n",
			buf, if_type, if_method);
	}

out:
	free(if_type);
	free(if_method);
	if (if_addr)
		free(if_addr);
	if (if_mask)
		free(if_mask);
	if (if_gw)
		free(if_gw);
}

static void
rumprun_deconfig_net(const char *if_index)
{
#if 1
	/* TODO According to pwwka this is not fully implemented yet */
	printf("rumprun_deconfig: (not yet) deconfiguring xenif%s\n", if_index);
#else
	char *if_type = NULL;
	char *if_method = NULL;
	char *if_addr = NULL;
	char *if_mask = NULL;
	char *if_gw = NULL;
	char buf[128];
	int rv;

	rv = xs_read_netconfig(if_index, &if_type, &if_method, &if_addr,
		&if_mask, &if_gw);
	if (rv != 0)
		return;

	if (strcmp(if_type, "inet") == 0 &&
	    strcmp(if_method, "dhcp") == 0) {
		/* TODO: need an interface into brlib dhcp to allow us to
		 * destroy the interface. */
	        printf("rumprun_deconfig: not deconfiguring xenif%s (uses dhcp)\n",
			if_index);
	}
	else if (strcmp(if_type, "inet") == 0 &&
		 strcmp(if_method, "static") == 0) {
		snprintf(buf, sizeof buf, "xenif%s", if_index);
		if ((rv = rump_pub_netconfig_ifdown(buf)) != 0) {
			warnx("rumprun_deconfig: %s: ifdown failed: %s\n", buf,
				strerror(rv));
			goto out;
		}
		if ((rv = rump_pub_netconfig_ifdestroy(buf)) != 0) {
			printf("rumprun_deconfig: %s: ifdestroy failed: %s\n",
				buf, strerror(rv));
			goto out;
		}
	}
	else {
		warnx("rumprun_config: %s: unknown type/method %s/%s\n",
			buf, if_type, if_method);
	}

out:
	free(if_type);
	free(if_method);
	if (if_addr)
		free(if_addr);
	if (if_mask)
		free(if_mask);
	if (if_gw)
		free(if_gw);
#endif
}

static int
xs_read_blkconfig(const char *blk_index, char **type, char **mountpoint,
	char **fstype)
{
	char *blk_type = NULL;
	char *blk_mountpoint = NULL;
	char *blk_fstype = NULL;
	char buf[128];
	char *xberr = NULL;
	xenbus_transaction_t txn;
	int xbretry = 0;
	
	xberr = xenbus_transaction_start(&txn);
	if (xberr) {
		warnx("rumprun_config: xenbus_transaction_start() failed: %s",
			xberr);
		return 1;
	}
	snprintf(buf, sizeof buf, "rumprun/blk/%s/type", blk_index);
	xberr = xenbus_read(txn, buf, &blk_type);
	if (xberr) {
		warnx("rumprun_config: xenblk%s: read %s failed: %s",
			blk_index, buf, xberr);
		xenbus_transaction_end(txn, 0, &xbretry);
		return 1;
	}
	if (strcmp(blk_type, "etfs") != 0) {
		warnx("rumprun_config: xenblk%s: unknown type '%s'",
			blk_index, blk_type);
		xenbus_transaction_end(txn, 0, &xbretry);
		free(blk_type);
		return 1;
	}
	snprintf(buf, sizeof buf, "rumprun/blk/%s/mountpoint", blk_index);
	xberr = xenbus_read(txn, buf, &blk_mountpoint);
	if (xberr) {
		warnx("rumprun_config: xenblk%s: read %s failed: %s",
			blk_index, buf, xberr);
		xenbus_transaction_end(txn, 0, &xbretry);
		free(blk_type);
		return 1;
	}
	snprintf(buf, sizeof buf, "rumprun/blk/%s/fstype", blk_index);
	xberr = xenbus_read(txn, buf, &blk_fstype);
	if (xberr) {
		warnx("rumprun_config: xenblk%s: read %s failed: %s",
			blk_index, buf, xberr);
		xenbus_transaction_end(txn, 0, &xbretry);
		free(blk_type);
		free(blk_mountpoint);
		return 1;
	}
	xberr = xenbus_transaction_end(txn, 0, &xbretry);
	if (xberr) {
		warnx("rumprun_config: xenbus_transaction_end() failed: %s",
			xberr);
		free(blk_type);
		free(blk_mountpoint);
		free(blk_fstype);
		return 1;
	}
	*type = blk_type;
	*mountpoint = blk_mountpoint;
	*fstype = blk_fstype;
	return 0;
}

static void
rumprun_config_blk(const char *blk_index)
{
	char *blk_type = NULL;
	char *blk_mountpoint = NULL;
	char *blk_fstype = NULL;
	int rv;
	char key[32],
	     hostpath[32];

	rv = xs_read_blkconfig(blk_index, &blk_type, &blk_mountpoint,
		&blk_fstype);
	if (rv != 0)
		return;
	if ((strcmp(blk_fstype, "ffs") != 0) &&
		(strcmp(blk_fstype, "cd9660") != 0)) {
		warnx("rumprun_config: xenblk%s: unsupported fstype %s\n",
			blk_index, blk_fstype);
		goto out;
	}
	
	printf("rumprun_config: mounting xenblk%s on %s as %s\n",
		blk_index, blk_mountpoint, blk_fstype);
	if ((rv = mkdir(blk_mountpoint, 0777)) != 0) {
		warnx("rumprun_config: mkdir failed: %d", errno);
		goto out;
	}
	snprintf(key, sizeof key, "/dev/xenblk%s", blk_index);
	snprintf(hostpath, sizeof hostpath, "blk%s", blk_index);
	if ((rv = rump_pub_etfs_register(key, hostpath, RUMP_ETFS_BLK)) != 0) {
		warnx("rumprun_config: etfs_register failed: %d", rv);
		goto out;
	}
	if (strcmp(blk_fstype, "ffs") == 0) {
		struct ufs_args mntargs = { .fspec = key };
		if (mount(MOUNT_FFS, blk_mountpoint, 0, &mntargs, sizeof(mntargs)) != 0) {
			warn("rumprun_config: mount_ffs failed");
			goto out;
		}
	}
	else if(strcmp(blk_fstype, "cd9660") == 0) {
		struct iso_args mntargs = { .fspec = key };
		if (mount(MOUNT_CD9660, blk_mountpoint, MNT_RDONLY, &mntargs, sizeof(mntargs)) != 0) {
			warn("rumprun_config: mount_cd9660 failed");
			goto out;
		}
	}

out:
	free(blk_type);
	free(blk_mountpoint);
	free(blk_fstype);
}

static void
rumprun_deconfig_blk(const char *blk_index)
{
	char *blk_type = NULL;
	char *blk_mountpoint = NULL;
	char *blk_fstype = NULL;
	char key[32];
	int rv;
	
	rv = xs_read_blkconfig(blk_index, &blk_type, &blk_mountpoint,
		&blk_fstype);
	if (rv != 0)
		return;
	
	printf("rumprun_config: unmounting xenblk%s from %s\n",
		blk_index, blk_mountpoint);
	if (unmount(blk_mountpoint, 0) != 0) {
		warnx("rumprun_config: unmount failed: %d", errno);
		/* Continue anyway, the initial mount may have failed. */
	}
	snprintf(key, sizeof key, "/dev/xenblk%s", blk_index);
	if ((rv = rump_pub_etfs_remove(key)) != 0) {
		warnx("rumprun_config: etfs_remove failed: %d", rv);
		goto out;
	}

out:
	free(blk_type);
	free(blk_mountpoint);
	free(blk_fstype);
}

void
_rumprun_config(void)
{
	char *err = NULL;
	xenbus_transaction_t txn;
	char **netdevices = NULL,
	     **blkdevices = NULL;
	int retry = 0,
	    i;

	err = xenbus_transaction_start(&txn);
	if (err) {
		warnx("rumprun_config: xenbus_transaction_start() failed: %s",
			err);
		goto out_err;
	}
	err = xenbus_ls(txn, "rumprun/net", &netdevices);
	if (err && strcmp(err, "ENOENT") != 0) {
		warnx("rumprun_config: xenbus_ls(rumprun/net) failed: %s", err);
		xenbus_transaction_end(txn, 0, &retry);
		goto out_err;
	}
	err = xenbus_ls(txn, "rumprun/blk", &blkdevices);
	if (err && strcmp(err, "ENOENT") != 0) {
		warnx("rumprun_config: xenbus_ls(rumprun/blk) failed: %s", err);
		xenbus_transaction_end(txn, 0, &retry);
		goto out_err;
	}
	err = xenbus_transaction_end(txn, 0, &retry);
	if (err) {
		warnx("rumprun_config: xenbus_transaction_end() failed: %s",
			err);
		goto out_err;
	}
	if (netdevices) {
		for(i = 0; netdevices[i]; i++) {
			rumprun_config_net(netdevices[i]);
			free(netdevices[i]);
		}
		free(netdevices);
	}
	if (blkdevices) {
		for(i = 0; blkdevices[i]; i++) {
			rumprun_config_blk(blkdevices[i]);
			free(blkdevices[i]);
		}
		free(blkdevices);
	}
	return;

out_err:
	if (netdevices) {
		for(i = 0; netdevices[i]; i++)
			free(netdevices[i]);
		free(netdevices);
	}
	if (blkdevices) {
		for(i = 0; blkdevices[i]; i++)
			free(blkdevices[i]);
		free(blkdevices);
	}
}

void
_rumprun_deconfig(void)
{
	char *err = NULL;
	xenbus_transaction_t txn;
	char **netdevices = NULL,
	     **blkdevices = NULL;
	int retry = 0,
	    i;

	err = xenbus_transaction_start(&txn);
	if (err) {
		warnx("rumprun_config: xenbus_transaction_start() failed: %s",
			err);
		goto out_err;
	}
	err = xenbus_ls(txn, "rumprun/net", &netdevices);
	if (err && strcmp(err, "ENOENT") != 0) {
		warnx("rumprun_config: xenbus_ls(rumprun/net) failed: %s", err);
		xenbus_transaction_end(txn, 0, &retry);
		goto out_err;
	}
	err = xenbus_ls(txn, "rumprun/blk", &blkdevices);
	if (err && strcmp(err, "ENOENT") != 0) {
		warnx("rumprun_config: xenbus_ls(rumprun/blk) failed: %s", err);
		xenbus_transaction_end(txn, 0, &retry);
		goto out_err;
	}
	err = xenbus_transaction_end(txn, 0, &retry);
	if (err) {
		warnx("rumprun_config: xenbus_transaction_end() failed: %s",
			err);
		goto out_err;
	}
	if (netdevices) {
		for(i = 0; netdevices[i]; i++) {
			rumprun_deconfig_net(netdevices[i]);
			free(netdevices[i]);
		}
		free(netdevices);
	}
	if (blkdevices) {
		for(i = 0; blkdevices[i]; i++) {
			rumprun_deconfig_blk(blkdevices[i]);
			free(blkdevices[i]);
		}
		free(blkdevices);
	}
	return;

out_err:
	if (netdevices) {
		for(i = 0; netdevices[i]; i++)
			free(netdevices[i]);
		free(netdevices);
	}
	if (blkdevices) {
		for(i = 0; blkdevices[i]; i++)
			free(blkdevices[i]);
		free(blkdevices);
	}
}
