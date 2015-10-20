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
#include <rumprun-base/parseargs.h>

#include <bmk-core/jsmn.h>

/* helper macros */
#define T_SIZE(t) ((t)->end - (t)->start)
#define T_STR(t,d) ((t)->start + d)
#define T_PRINTFSTAR(t,d) T_SIZE(t), T_STR(t,d)
#define T_STREQ(t, d, str) (strncmp(T_STR(t,d), str, T_SIZE(t)) == 0)

#define T_STRCPY(dest, destsize, t, d)					\
  do {									\
	unsigned long strsize = MIN(destsize-1,T_SIZE(t));		\
	strncpy(dest, T_STR(t, d), strsize);				\
	dest[strsize] = '\0';						\
  } while (/*CONSTCOND*/0)

#define T_CHECKTYPE(t, data, exp, fun)					\
  do {									\
	if (t->type != exp) {						\
		errx(1, "unexpected type for token \"%.*s\" "		\
		    "in \"%s\"", T_PRINTFSTAR(t,data), fun);		\
	}								\
  } while (/*CONSTCOND*/0)

#define T_CHECKSIZE(t, data, exp, fun)					\
  do {									\
	if (t->size != exp) {						\
		errx(1, "unexpected size for token \"%.*s\" "		\
		    "in \"%s\"", T_PRINTFSTAR(t,data), fun);		\
	}								\
  } while (/*CONSTCOND*/0)

static char *
token2cstr(jsmntok_t *t, char *data)
{

	*(T_STR(t, data) + T_SIZE(t)) = '\0';
	return T_STR(t, data);
}

struct rumprun_execs rumprun_execs = TAILQ_HEAD_INITIALIZER(rumprun_execs);

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
	rre->rre_flags = 0;
	rre->rre_argc = nargs;

	TAILQ_INSERT_TAIL(&rumprun_execs, rre, rre_entries);
}

static int
handle_cmdline(jsmntok_t *t, int left, char *data)
{

	T_CHECKTYPE(t, data, JSMN_STRING, __func__);

	makeargv(token2cstr(t, data));

	return 1;
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
static int
addbin(jsmntok_t *t, char *data)
{
	jsmntok_t *t_bin, *t_argv, *t_runmode;
	struct rumprun_exec *rre;
	jsmntok_t *key, *value;
	char *binname;
	int binsize = 1;
	int objleft = t->size;
	int rreflags, i;

	T_CHECKTYPE(t, data, JSMN_OBJECT, __func__);
	t++;

	/* process and validate data */
	t_bin = t_argv = t_runmode = NULL;
	while (objleft--) {
		int mysize;

		key = t;
		value = t+1;

		T_CHECKTYPE(key, data, JSMN_STRING, __func__);

		if (T_STREQ(key, data, "bin")) {
			t_bin = value;

			T_CHECKSIZE(key, data, 1, __func__);
			T_CHECKTYPE(value, data, JSMN_STRING, __func__);
			T_CHECKSIZE(value, data, 0, __func__);

			mysize = 1 + 1;
		} else if (T_STREQ(key, data, "argv")) {
			T_CHECKTYPE(value, data, JSMN_ARRAY, __func__);

			t_argv = value;

			/* key + array + array contents */
			mysize = 1 + 1 + value->size;
		} else if (T_STREQ(key, data, "runmode")) {
			t_runmode = value;

			T_CHECKSIZE(key, data, 1, __func__);
			T_CHECKTYPE(value, data, JSMN_STRING, __func__);
			T_CHECKSIZE(value, data, 0, __func__);

			mysize = 1 + 1;
		} else {
			errx(1, "unexpected key \"%.*s\" in \"%s\"",
			    T_PRINTFSTAR(key, data), __func__);
		}

		t += mysize;
		binsize += mysize;
	}

	if (!t_bin)
		errx(1, "missing \"bin\" for rc entry");
	binname = token2cstr(t_bin, data);

	if (t_runmode) {
		bool sizeok = T_SIZE(t_runmode) == 1;

		if (sizeok && *T_STR(t_runmode,data) == '|') {
			rreflags = RUMPRUN_EXEC_PIPE;
		} else if (sizeok && *T_STR(t_runmode,data) == '&') {
			rreflags = RUMPRUN_EXEC_BACKGROUND;
		} else {
			errx(1, "invalid runmode \"%.*s\" for bin \"%.*s\"",
			    T_PRINTFSTAR(t_runmode, data),
			    T_PRINTFSTAR(t_bin, data));
		}
	} else {
		rreflags = 0;
	}

	/* ok, we got everything.  save into rumprun_exec structure */
	rre = malloc(sizeof(*rre) + (2+t_argv->size) * sizeof(char *));
	if (rre == NULL)
		err(1, "allocate rumprun_exec");
	rre->rre_flags = rreflags;
	rre->rre_argc = 1+t_argv->size;
	rre->rre_argv[0] = binname;
	for (i = 1, t = t_argv+1; i <= t_argv->size; i++, t++) {
		T_CHECKTYPE(t, data, JSMN_STRING, __func__);
		rre->rre_argv[i] = token2cstr(t, data);
	}
	rre->rre_argv[rre->rre_argc] = NULL;

	TAILQ_INSERT_TAIL(&rumprun_execs, rre, rre_entries);

	return binsize;
}

static int
handle_rc(jsmntok_t *t, int left, char *data)
{
	int onesize, totsize;

	T_CHECKTYPE(t, data, JSMN_ARRAY, __func__);

	totsize = 1;
	t++;
	left--;

	while (left) {
		onesize = addbin(t, data);
		left -= onesize;
		totsize += onesize;
		t += onesize;
	}

	return totsize;
}

static int
handle_env(jsmntok_t *t, int left, char *data)
{

	T_CHECKTYPE(t, data, JSMN_STRING, __func__);

	if (putenv(token2cstr(t, data)) == -1)
		err(1, "putenv");

	return 1;
}

static int
handle_hostname(jsmntok_t *t, int left, char *data)
{

	T_CHECKTYPE(t, data, JSMN_STRING, __func__);

	if (sethostname(token2cstr(t, data), T_SIZE(t)) == -1)
		err(1, "sethostname");

	return 1;
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

static int
handle_net(jsmntok_t *t, int left, char *data)
{
	const char *ifname, *cloner, *type, *method;
	const char *addr, *mask, *gw;
	jsmntok_t *key, *value;
	int i, objsize;
	int rv;
	static int configured;

	T_CHECKTYPE(t, data, JSMN_OBJECT, __func__);

	/* we expect straight key-value pairs (at least for now) */
	objsize = t->size;
	if (left < 2*objsize + 1) {
		return -1;
	}
	t++;

	if (configured) {
		errx(1, "currently only 1 \"net\" configuration is supported");
	}

	ifname = cloner = type = method = NULL;
	addr = mask = gw = NULL;

	for (i = 0; i < objsize; i++, t+=2) {
		const char *valuestr;
		key = t;
		value = t+1;

		T_CHECKTYPE(key, data, JSMN_STRING, __func__);
		T_CHECKSIZE(key, data, 1, __func__);

		T_CHECKTYPE(value, data, JSMN_STRING, __func__);
		T_CHECKSIZE(value, data, 0, __func__);

		/*
		 * XXX: this mimics the structure from Xen.  We probably
		 * want a richer structure, but let's be happy to not
		 * diverge for now.
		 */
		valuestr = token2cstr(value, data);
		if (T_STREQ(key, data, "if")) {
			ifname = valuestr;
		} else if (T_STREQ(key, data, "cloner")) {
			cloner = valuestr;
		} else if (T_STREQ(key, data, "type")) {
			type = valuestr;
		} else if (T_STREQ(key, data, "method")) {
			method = valuestr;
		} else if (T_STREQ(key, data, "addr")) {
			addr = valuestr;
		} else if (T_STREQ(key, data, "mask")) {
			/* XXX: we could also pass mask as a number ... */
			mask = valuestr;
		} else if (T_STREQ(key, data, "gw")) {
			gw = valuestr;
		} else {
			errx(1, "unexpected key \"%.*s\" in \"%s\"",
			    T_PRINTFSTAR(key, data), __func__);
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

	return 2*objsize + 1;
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

static int
handle_blk(jsmntok_t *t, int left, char *data)
{
	const char *source, *origpath, *fstype;
	char *mp, *path;
	jsmntok_t *key, *value;
	int i, objsize;

	T_CHECKTYPE(t, data, JSMN_OBJECT, __func__);

	/* we expect straight key-value pairs */
	objsize = t->size;
	if (left < 2*objsize + 1) {
		return -1;
	}
	t++;

	fstype = source = origpath = mp = path = NULL;

	for (i = 0; i < objsize; i++, t+=2) {
		char *valuestr;
		key = t;
		value = t+1;

		T_CHECKTYPE(key, data, JSMN_STRING, __func__);
		T_CHECKSIZE(key, data, 1, __func__);

		T_CHECKTYPE(value, data, JSMN_STRING, __func__);
		T_CHECKSIZE(value, data, 0, __func__);

		valuestr = token2cstr(value, data);
		if (T_STREQ(key, data, "source")) {
			source = valuestr;
		} else if (T_STREQ(key, data, "path")) {
			origpath = path = valuestr;
		} else if (T_STREQ(key, data, "fstype")) {
			fstype = valuestr;
		} else if (T_STREQ(key, data, "mountpoint")) {
			mp = valuestr;
		} else {
			errx(1, "unexpected key \"%.*s\" in \"%s\"",
			    T_PRINTFSTAR(key, data), __func__);
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

	return 2*objsize + 1;
}

struct {
	const char *name;
	int (*handler)(jsmntok_t *, int, char *);
} parsers[] = {
	{ "cmdline", handle_cmdline },
	{ "rc_TESTING", handle_rc },
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
	jsmn_parser p;
	jsmntok_t *tokens = NULL;
	jsmntok_t *t;
	size_t cmdline_len;
	unsigned int i;
	int ntok;

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

	cmdline_len = strlen(cmdline);
	jsmn_init(&p);
	ntok = jsmn_parse(&p, cmdline, cmdline_len, NULL, 0);

	if (ntok <= 0) {
		errx(1, "json parse failed 1");
	}

	tokens = malloc(ntok * sizeof(*t));
	if (!tokens) {
		errx(1, "failed to allocate jsmn tokens");
	}

	jsmn_init(&p);
	if ((ntok = jsmn_parse(&p, cmdline, cmdline_len, tokens, ntok)) < 1) {
		errx(1, "json parse failed 2");
	}

	T_CHECKTYPE(tokens, cmdline, JSMN_OBJECT, __func__);

	for (t = &tokens[1]; t < &tokens[ntok]; ) {
		for (i = 0; i < __arraycount(parsers); i++) {
			if (T_STREQ(t, cmdline, parsers[i].name)) {
				int left;

				t++;
				left = &tokens[ntok] - t;
				t += parsers[i].handler(t, left, cmdline);
				break;
			}
		}
		if (i == __arraycount(parsers))
			errx(1, "no match for key \"%.*s\"",
			    T_PRINTFSTAR(t, cmdline));
	}

	free(tokens);
}
