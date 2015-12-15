/*-
 * Copyright (c) 2015 Antti Kantee.  All Rights Reserved.
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

/*
 * NOTE: this implementation is currently a sketch of what things
 * should looks like.
 */

#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <ufs/ufs/ufsmount.h>
#include <isofs/cd9660/cd9660_mount.h>

#include <dev/vndvar.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rump/rump.h>
#include <rump/netconfig.h>

#include <rumprun-base/config.h>
#include <rumprun-base/json.h>
#include <rumprun-base/parseargs.h>

struct rumprun_execs rumprun_execs = TAILQ_HEAD_INITIALIZER(rumprun_execs);

static const char *
jtypestr(enum jtypes t)
{

	switch (t) {
	case jnull:	return "NULL";
	case jtrue:	return "BOOLEAN";
	case jfalse:	return "BOOLEAN";
	case jstring:	return "STRING";
	case jarray:	return "ARRAY";
	case jobject:	return "OBJECT";
	default:	return "UNKNOWN";
	}
}

static void
jexpect(enum jtypes t, jvalue *v, const char *loc)
{

	if (v->d != t)
		warnx("%s: expected %s, got %s", loc, jtypestr(t),
			jtypestr(v->d));
}

static void
makeargv(char *argvstr)
{
	struct rumprun_exec *rre;
	char **argv;
	int nargs;

	rumprun_parseargs(argvstr, &nargs, 0);
	rre = malloc(sizeof(*rre) + (nargs+1) * sizeof(*argv));
	if (rre == NULL)
		err(1, "could not allocate rre");

	rumprun_parseargs(argvstr, &nargs, rre->rre_argv);
	rre->rre_argv[nargs] = NULL;
	rre->rre_flags = RUMPRUN_EXEC_CMDLINE;
	rre->rre_argc = nargs;

	TAILQ_INSERT_TAIL(&rumprun_execs, rre, rre_entries);
}

static void
handle_cmdline(jvalue *v, const char *loc)
{

	jexpect(jstring, v, __func__);
	makeargv(strdup(v->u.s));
}

/*
 * "rc": [
 *	{ "bin" : "binname",
 *	  "argv" : [ "arg1", "arg2", ... ], (optional)
 *	  "runmode" : "& OR |" (optional)
 *	},
 *      ....
 * ]
 */
static void
addbin(jvalue *v, const char *loc)
{
	struct rumprun_exec *rre;
	char *binname;
	jvalue *v_bin, *v_argv, *v_runmode;
	int rreflags;
	size_t nargv;

	jexpect(jobject, v, __func__);

	/* process and validate data */
	v_bin = v_argv = v_runmode = NULL;
	for (jvalue **i = v->u.v; *i; ++i) {

		if (strcmp((*i)->n, "bin") == 0) {
			jexpect(jstring, *i, __func__);
			v_bin = *i;
		} else if (strcmp((*i)->n, "argv") == 0) {
			jexpect(jarray, *i, __func__);
			v_argv = *i;
		} else if (strcmp((*i)->n, "runmode") == 0) {
			jexpect(jstring, *i, __func__);
			v_runmode = *i;
		} else {
			errx(1, "unexpected key \"%s\" in \"%s\"", (*i)->n,
				__func__);
		}
	}

	if (!v_bin)
		errx(1, "missing \"bin\" for rc entry");
	binname = strdup(v_bin->u.s);

	if (v_runmode) {
		if (strcmp(v_runmode->u.s, "&") == 0) {
			rreflags = RUMPRUN_EXEC_BACKGROUND;
		} else if (strcmp(v_runmode->u.s, "|") == 0) {
			rreflags = RUMPRUN_EXEC_PIPE;
		} else {
			errx(1, "invalid runmode \"%s\" for bin \"%s\"",
				v_runmode->u.s, v_bin->u.s);
		}
	} else {
		rreflags = 0;
	}

	nargv = 0;
	if (v_argv) {
		for (jvalue **i = v_argv->u.v; *i; ++i) {
			jexpect(jstring, *i, __func__);
			nargv++;
		}
	}

	/* ok, we got everything.  save into rumprun_exec structure */
	rre = malloc(sizeof(*rre) + (2+nargv) * sizeof(char *));
	if (rre == NULL)
		err(1, "allocate rumprun_exec");
	rre->rre_flags = rreflags;
	rre->rre_argc = 1+nargv;
	rre->rre_argv[0] = binname;
	int j; jvalue **arg; /* XXX decl? */
	for (j = 1, arg = v_argv->u.v; *arg; ++arg, ++j) {
		rre->rre_argv[j] = strdup((*arg)->u.s);
	}
	rre->rre_argv[rre->rre_argc] = NULL;

	TAILQ_INSERT_TAIL(&rumprun_execs, rre, rre_entries);
}

static void
handle_rc(jvalue *v, const char *loc)
{

	jexpect(jarray, v, __func__);
	for (jvalue **i = v->u.v; *i; ++i)
		addbin(v, __func__);
}

static void
handle_env(jvalue *v, const char *loc)
{

	jexpect(jstring, v, __func__);
	if (putenv(v->u.s) == -1)
		err(1, "putenv");
}

static void
handle_hostname(jvalue *v, const char *loc)
{

	jexpect(jstring, v, __func__);
	if (sethostname(v->u.s, strlen(v->u.s)) == -1)
		err(1, "sethostname");
}

static void
config_ipv4(const char *ifname, const char *method,
	const char *addr, const char *mask, const char *gw)
{
	int rv;

	if (strcmp(method, "dhcp") == 0) {
		if ((rv = rump_pub_netconfig_dhcp_ipv4_oneshot(ifname)) != 0)
			errx(1, "configuring dhcp for %s failed: %d",
			    ifname, rv);
	} else {
		if (strcmp(method, "static") != 0) {
			errx(1, "method \"static\" or \"dhcp\" expected, "
			    "got \"%s\"", method);
		}

		if (!addr || !mask) {
			errx(1, "static net cfg missing addr or mask");
		}

		if ((rv = rump_pub_netconfig_ipv4_ifaddr_cidr(ifname,
		    addr, atoi(mask))) != 0) {
			errx(1, "ifconfig \"%s\" for \"%s/%s\" failed",
			    ifname, addr, mask);
		}
		if (gw && (rv = rump_pub_netconfig_ipv4_gw(gw)) != 0) {
			errx(1, "gw \"%s\" addition failed", gw);
		}
	}
}

static void
config_ipv6(const char *ifname, const char *method,
	const char *addr, const char *mask, const char *gw)
{
	int rv;

	if (strcmp(method, "auto") == 0) {
		if ((rv = rump_pub_netconfig_auto_ipv6(ifname)) != 0) {
			errx(1, "ipv6 autoconfig failed");
		}
	} else {
		if (strcmp(method, "static") != 0) {
			errx(1, "method \"static\" or \"dhcp\" expected, "
			    "got \"%s\"", method);
		}

		if (!addr || !mask) {
			errx(1, "static net cfg missing addr or mask");
		}

		if ((rv = rump_pub_netconfig_ipv6_ifaddr(ifname,
		    addr, atoi(mask))) != 0) {
			errx(1, "ifconfig \"%s\" for \"%s/%s\" failed",
			    ifname, addr, mask);
		}
		if (gw && (rv = rump_pub_netconfig_ipv6_gw(gw)) != 0) {
			errx(1, "gw \"%s\" addition failed", gw);
		}
	}
}

static void
handle_net(jvalue *v, const char *loc)
{
	const char *ifname, *cloner, *type, *method;
	const char *addr, *mask, *gw;
	int rv;

	jexpect(jobject, v, __func__);

	ifname = cloner = type = method = NULL;
	addr = mask = gw = NULL;

	for (jvalue **i = v->u.v; *i; ++i) {
		jexpect(jstring, *i, __func__);

		/*
		 * XXX: this mimics the structure from Xen.  We probably
		 * want a richer structure, but let's be happy to not
		 * diverge for now.
		 */
		if (strcmp((*i)->n, "if") == 0) {
			ifname = (*i)->u.s;
		} else if (strcmp((*i)->n, "cloner") == 0) {
			cloner = (*i)->u.s;
		} else if (strcmp((*i)->n, "type") == 0) {
			type = (*i)->u.s;
		} else if (strcmp((*i)->n, "method") == 0) {
			method = (*i)->u.s;
		} else if (strcmp((*i)->n, "addr") == 0) {
			addr = (*i)->u.s;
		} else if (strcmp((*i)->n, "mask") == 0) {
			/* XXX: we could also pass mask as a number ... */
			mask = (*i)->u.s;
		} else if (strcmp((*i)->n, "gw") == 0) {
			gw = (*i)->u.s;
		} else {
			errx(1, "unexpected key \"%s\" in \"%s\"", (*i)->n,
				__func__);
		}
	}

	if (!ifname || !type || !method) {
		errx(1, "net cfg missing vital data, not configuring");
	}

	if (cloner) {
		if ((rv = rump_pub_netconfig_ifcreate(ifname)) != 0) {
			errx(1, "rumprun_config: ifcreate %s failed: %d",
			    ifname, rv);
		}
	}

	if (strcmp(type, "inet") == 0) {
		config_ipv4(ifname, method, addr, mask, gw);
	} else if (strcmp(type, "inet6") == 0) {
		config_ipv6(ifname, method, addr, mask, gw);
	} else {
		errx(1, "network type \"%s\" not supported", type);
	}
}

static void
makevnddev(int israw, int unit, int part, char *storage, size_t storagesize)
{

	snprintf(storage, storagesize, "/dev/%svnd%d%c",
	    israw ? "r" : "", unit, 'a' + part);
}

static devmajor_t
getvndmajor(int israw)
{
	struct stat sb;
	char path[32];

	makevnddev(israw, 0, RAW_PART, path, sizeof(path));
	if (stat(path, &sb) == -1)
		err(1, "failed to stat %s", path);
	return major(sb.st_rdev);
}

static char *
configvnd(const char *path)
{
	static int nextvnd;
	struct vnd_ioctl vndio;
	char bbuf[32], rbuf[32];
	int fd;

	makevnddev(0, nextvnd, RAW_PART, bbuf, sizeof(bbuf));
	makevnddev(1, nextvnd, RAW_PART, rbuf, sizeof(rbuf));

	memset(&vndio, 0, sizeof(vndio));
	vndio.vnd_file = __UNCONST(path);
	vndio.vnd_flags = VNDIOF_READONLY;

	fd = open(rbuf, O_RDWR);
	if (fd == -1) {
		/*
		 * node doesn't exist?  try creating it.  use majors from
		 * vnd0, which we (obviously) assume/hope exists
		 */
		if (errno == ENOENT) {
			const devmajor_t bmaj = getvndmajor(0);
			const devmajor_t rmaj = getvndmajor(1);

			if (mknod(bbuf, 0666 | S_IFBLK,
			    MAKEDISKDEV(bmaj, nextvnd, RAW_PART)) == -1)
				err(1, "mknod %s", bbuf);
			if (mknod(rbuf, 0666 | S_IFBLK,
			    MAKEDISKDEV(rmaj, nextvnd, RAW_PART)) == -1)
				err(1, "mknod %s", rbuf);

			fd = open(rbuf, O_RDWR);
		}
		if (fd == -1)
			err(1, "cannot open %s", rbuf);
	}

	if (ioctl(fd, VNDIOCSET, &vndio) == -1)
		err(1, "vndset failed");
	close(fd);

	nextvnd++;
	return strdup(bbuf);
}

static char *
configetfs(const char *path, int hard)
{
	char buf[32];
	char epath[32];
	char *p;
	int rv;

	snprintf(epath, sizeof(epath), "XENBLK_%s", path);
	snprintf(buf, sizeof(buf), "/dev/%s", path);
	rv = rump_pub_etfs_register(buf, epath, RUMP_ETFS_BLK);
	if (rv != 0) {
		if (!hard)
			return NULL;
		errx(1, "etfs register for \"%s\" failed: %d", path, rv);
	}

	if ((p = strdup(buf)) == NULL)
		err(1, "failed to allocate pathbuf");
	return p;
}

static bool
mount_blk(const char *dev, const char *mp)
{
	struct ufs_args mntargs_ufs = { .fspec = __UNCONST(dev) };
	struct iso_args mntargs_iso = { .fspec = dev };

	if (mount(MOUNT_FFS, mp, 0, &mntargs_ufs, sizeof(mntargs_ufs)) == 0)
		return true;
	if (mount(MOUNT_EXT2FS, mp, 0, &mntargs_ufs, sizeof(mntargs_ufs)) == 0)
		return true;
	if (mount(MOUNT_CD9660,
	    mp, MNT_RDONLY, &mntargs_iso, sizeof(mntargs_iso)) == 0)
		return true;

	return false;
}

static bool
mount_kernfs(const char *dev, const char *mp)
{

	if (mount(MOUNT_KERNFS, mp, 0, NULL, 0) == 0)
		return true;

	return false;
}

struct {
	const char *mt_fstype;
	bool (*mt_mount)(const char *, const char *);
} mounters[] = {
	{ "blk",	mount_blk },
	{ "kernfs",	mount_kernfs },
};

static void
handle_blk(jvalue *v, const char *loc)
{
	const char *source, *origpath, *fstype;
	char *mp, *path;

	jexpect(jobject, v, __func__);

	fstype = source = origpath = mp = path = NULL;

	for (jvalue **i = v->u.v; *i; ++i) {
		jexpect(jstring, *i, __func__);

		if (strcmp((*i)->n, "source") == 0) {
			source = (*i)->u.s;
		} else if (strcmp((*i)->n, "path") == 0) {
			origpath = path = (*i)->u.s;
		} else if (strcmp((*i)->n, "fstype") == 0) {
			fstype = (*i)->u.s;
		} else if (strcmp((*i)->n, "mountpoint") == 0) {
			mp = (*i)->u.s;
		} else {
			errx(1, "unexpected key \"%s\" in \"%s\"", (*i)->n,
				__func__);
		}
	}

	if (!source || !path) {
		errx(1, "blk cfg missing vital data");
	}

	if (strcmp(source, "dev") == 0) {
		/* nothing to do here */
	} else if (strcmp(source, "vnd") == 0) {
		path = configvnd(path);
	} else if (strcmp(source, "etfs") == 0) {
		path = configetfs(path, 1);
	} else {
		errx(1, "unsupported blk source \"%s\"", source);
	}

	/* we only need to do something only if a mountpoint is specified */
	if (mp) {
		char *chunk;
		unsigned mi;

		if (!fstype) {
			errx(1, "no fstype for mountpoint \"%s\"\n", mp);
		}

		for (chunk = mp;;) {
			bool end;

			/* find & terminate the next chunk */
			chunk += strspn(chunk, "/");
			chunk += strcspn(chunk, "/");
			end = (*chunk == '\0');
			*chunk = '\0';

			if (mkdir(mp, 0755) == -1) {
				if (errno != EEXIST)
					err(1, "failed to create mp dir \"%s\"",
					    chunk);
			}

			/* restore path */
			if (!end)
				*chunk = '/';
			else
				break;
		}

		for (mi = 0; mi < __arraycount(mounters); mi++) {
			if (strcmp(fstype, mounters[mi].mt_fstype) == 0) {
				if (!mounters[mi].mt_mount(path, mp))
					errx(1, "failed to mount fs type "
					    "\"%s\" from \"%s\" to \"%s\"",
					    fstype, path, mp);
				break;
			}
		}
		if (mi == __arraycount(mounters))
			errx(1, "unknown fstype \"%s\"", fstype);
	}

	if (path != origpath)
		free(path);
}

struct {
	const char *name;
	void (*handler)(jvalue *, const char *);
} parsers[] = {
	{ "cmdline", handle_cmdline },
	{ "rc", handle_rc },
	{ "env", handle_env },
	{ "hostname", handle_hostname },
	{ "blk", handle_blk },
	{ "net", handle_net },
};

/* don't believe we can have a >64k config */
#define CFGMAXSIZE (64*1024)
static char *
getcmdlinefromroot(const char *cfgname)
{
	const char *tryroot[] = {
		"/dev/ld0a",
		"/dev/sd0a",
	};
	struct stat sb;
	unsigned int i;
	int fd;
	char *p;

	if (mkdir("/rootfs", 0777) == -1)
		err(1, "mkdir /rootfs failed");

	/*
	 * XXX: should not be hardcoded to cd9660.  but it is for now.
	 * Maybe use mountroot() here somehow?
	 */
	for (i = 0; i < __arraycount(tryroot); i++) {
		if (mount_blk(tryroot[i], "/rootfs"))
			break;
	}

	/* didn't find it that way.  one more try: etfs for sda1 (EC2) */
	if (i == __arraycount(tryroot)) {
		char *devpath;

		devpath = configetfs("sda1", 0);
		if (!devpath)
			errx(1, "failed to mount rootfs from image");

		if (!mount_blk(devpath, "/rootfs"))
			errx(1, "failed to mount /rootfs");
	}

	/*
	 * Ok, we've successfully mounted /rootfs.  Now get the config.
	 */

	while (*cfgname == '/')
		cfgname++;
	if (chdir("/rootfs") == -1)
		err(1, "chdir rootfs");

	if ((fd = open(cfgname, O_RDONLY)) == -1)
		err(1, "open %s", cfgname);
	if (stat(cfgname, &sb) == -1)
		err(1, "stat %s", cfgname);

	if (sb.st_size > CFGMAXSIZE)
		errx(1, "unbelievable cfg file size, increase CFGMAXSIZE");
	if ((p = malloc(sb.st_size+1)) == NULL)
		err(1, "cfgname storage");

	if (read(fd, p, sb.st_size) != sb.st_size)
		err(1, "read cfgfile");
	close(fd);

	p[sb.st_size] = '\0';
	return p;
}


#define ROOTCFG "_RUMPRUN_ROOTFSCFG="
static const size_t rootcfglen = sizeof(ROOTCFG)-1;
static char *
rumprun_config_path(char *cmdline)
{
	char *cfg = strstr(cmdline, ROOTCFG);

	if (cfg != NULL)
		cfg += rootcfglen;

	return cfg;
}
#undef ROOTCFG

void
rumprun_config(char *cmdline)
{
	char *cfg;
	struct rumprun_exec *rre;
	jvalue *root;

	/* is the config file on rootfs?  if so, mount & dig it out */
	cfg = rumprun_config_path(cmdline);
	if (cfg != NULL) {
		cmdline = getcmdlinefromroot(cfg);
		if (cmdline == NULL)
			errx(1, "could not get cfg from rootfs");
	}

	while (*cmdline != '{') {
		if (*cmdline == '\0') {
			warnx("could not find start of json.  no config?");
			makeargv(strdup("rumprun"));
			return;
		}
		cmdline++;
	}

	root = jparse(cmdline);
	if (!root)
		errx(1, "jparse failed");
	jexpect(jobject, root, __func__);

	for (jvalue **i = root->u.v; *i; ++i) {

		size_t j;
		for (j = 0; j < __arraycount(parsers); j++) {
			if (strcmp((*i)->n, parsers[j].name) == 0) {
				parsers[j].handler(*i, __func__);
				break;
			}
		}
		if (j == __arraycount(parsers))
			errx(1, "no match for key \"%s\"", (*i)->n);
	}

	/*
	 * Before we start running things, perform some sanity checks
	 */
	rre = TAILQ_LAST(&rumprun_execs, rumprun_execs);
	if (rre == NULL) {
		errx(1, "rumprun_config: no bins");
	}
	if (rre->rre_flags & RUMPRUN_EXEC_PIPE) {
		errx(1, "rumprun_config: last bin may not output to pipe");
	}

	jdel(root);
}
