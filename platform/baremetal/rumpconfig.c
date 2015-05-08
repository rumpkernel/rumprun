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
#include <sys/stat.h>

#include <ufs/ufs/ufsmount.h>
#include <isofs/cd9660/cd9660_mount.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int rumprun_cmdline_argc;
char **rumprun_cmdline_argv;

static void
makeargv(char *argvstr)
{
	char **argv;
	int nargs;

	assert(rumprun_cmdline_argc == 0 && rumprun_cmdline_argv == NULL);

	rumprun_parseargs(argvstr, &nargs, 0);
	argv = malloc(sizeof(*argv) * (nargs+2));
	if (argv == NULL)
		errx(1, "could not allocate argv");

	rumprun_parseargs(argvstr, &nargs, argv);
	argv[nargs] = argv[nargs+1] = '\0';
	rumprun_cmdline_argv = argv;
	rumprun_cmdline_argc = nargs;
}

static int
handle_cmdline(jsmntok_t *t, int left, const char *data)
{
	char *argvstr;
	size_t argvstrlen;

	T_CHECKTYPE(t, data, JSMN_STRING, __func__);

	/* allocate memory for argv element storage */
	argvstrlen = T_SIZE(t)+1;
	argvstr = malloc(argvstrlen);
	if (argvstr == NULL)
		errx(1, "could not allocate argv storage");

	T_STRCPY(argvstr, argvstrlen, t, data);
	makeargv(argvstr);

	return 1;
}

static int
handle_env(jsmntok_t *t, int left, const char *data)
{
	char *envstr;

	T_CHECKTYPE(t, data, JSMN_STRING, __func__);

	envstr = malloc(T_SIZE(t)+1);
	if (envstr == NULL)
		err(1, "allocate env string");
	T_STRCPY(envstr, T_SIZE(t)+1, t, data);
	if (putenv(envstr) == -1)
		err(1, "putenv");

	return 1;
}

static int
handle_net(jsmntok_t *t, int left, const char *data)
{
	char ifname[32], type[32], method[32];
	char addr[32], mask[32], gw[32];
	jsmntok_t *key, *value;
	int rv, i, objsize;
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

	ifname[0] = type[0] = method[0] = '\0';
	addr[0] = mask[0] = gw[0] = '\0';

	for (i = 0; i < objsize; i++, t+=2) {
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
		if (T_STREQ(key, data, "if")) {
			T_STRCPY(ifname, sizeof(ifname), value, data);
		} else if (T_STREQ(key, data, "type")) {
			T_STRCPY(type, sizeof(type), value, data);
		} else if (T_STREQ(key, data, "method")) {
			T_STRCPY(method, sizeof(method), value, data);
		} else if (T_STREQ(key, data, "addr")) {
			T_STRCPY(addr, sizeof(addr), value, data);
		} else if (T_STREQ(key, data, "mask")) {
			/* XXX: we could also pass mask as a number ... */
			T_STRCPY(mask, sizeof(mask), value, data);
		} else if (T_STREQ(key, data, "gw")) {
			T_STRCPY(gw, sizeof(gw), value, data);
		} else {
			errx(1, "unexpected key \"%.*s\" in \"%s\", ignoring",
			    T_PRINTFSTAR(key, data), __func__);
		}
	}

	if (!ifname[0] || !type[0] || !method[0]) {
		errx(1, "net cfg missing vital data, not configuring");
	}

	if (strcmp(type, "inet") != 0) {
		errx(1, "only ipv4 is supported currently, got: \"%s\"", type);
	}

	if (strcmp(method, "dhcp") == 0) {
		if ((rv = rump_pub_netconfig_dhcp_ipv4_oneshot(ifname)) != 0)
			errx(1, "configuring dhcp for %s failed: %d",
			    ifname, rv);
	} else {
		if (strcmp(method, "static") != 0) {
			errx(1, "method \"static\" or \"dhcp\" expected, "
			    "got \"%s\"", method);
		}

		if (!addr[0] || !mask[0]) {
			errx(1, "static net cfg missing addr or mask");
		}

		if ((rv = rump_pub_netconfig_ipv4_ifaddr_cidr(ifname,
		    addr, atoi(mask))) != 0) {
			errx(1, "ifconfig \"%s\" for \"%s/%s\" failed",
			    ifname, addr, mask);
		}
		if (gw[0] && (rv = rump_pub_netconfig_ipv4_gw(gw)) != 0) {
			errx(1, "gw \"%s\" addition failed", gw);
		}
	}

	return 2*objsize + 1;
}

static int
handle_blk(jsmntok_t *t, int left, const char *data)
{
	char devname[64], fstype[16];
	char *mp;
	jsmntok_t *key, *value;
	int i, objsize;

	T_CHECKTYPE(t, data, JSMN_OBJECT, __func__);

	/* we expect straight key-value pairs */
	objsize = t->size;
	if (left < 2*objsize + 1) {
		return -1;
	}
	t++;

	fstype[0] = devname[0] = '\0';
	mp = NULL;

	for (i = 0; i < objsize; i++, t+=2) {
		key = t;
		value = t+1;

		T_CHECKTYPE(key, data, JSMN_STRING, __func__);
		T_CHECKSIZE(key, data, 1, __func__);

		T_CHECKTYPE(value, data, JSMN_STRING, __func__);
		T_CHECKSIZE(value, data, 0, __func__);

		if (T_STREQ(key, data, "dev")) {
			T_STRCPY(devname, sizeof(devname), value, data);
		} else if (T_STREQ(key, data, "fstype")) {
			T_STRCPY(fstype, sizeof(fstype), value, data);
		} else if (T_STREQ(key, data, "mountpoint")) {
			size_t mplen = T_SIZE(t);

			mp = malloc(mplen+1);
			if (mp == NULL)
				errx(1, "failed to allocate mountpoint path");

			T_STRCPY(mp, mplen+1, value, data);
		} else {
			errx(1, "unexpected key \"%.*s\" in \"%s\"",
			    T_PRINTFSTAR(key, data), __func__);
		}
	}

	if (!devname[0] || !fstype[0]) {
		errx(1, "blk cfg missing vital data");
	}

	/* we only need to do something only if a mountpoint is specified */
	if (mp) {
		/* XXX: handles only one component */
		if (mkdir(mp, 0777) == -1)
			errx(1, "creating mountpoint \"%s\" failed", mp);

		if (strcmp(fstype, "ffs") == 0) {
			struct ufs_args mntargs = { .fspec = devname };

			if (mount(MOUNT_FFS, mp, 0,
			    &mntargs, sizeof(mntargs)) == -1) {
				errx(1, "rumprun_config: mount_ffs failed");
			}
		} else if(strcmp(fstype, "cd9660") == 0) {
			struct iso_args mntargs = { .fspec = devname };

			if (mount(MOUNT_CD9660, mp, MNT_RDONLY,
			    &mntargs, sizeof(mntargs)) == -1) {
				errx(1, "rumprun_config: mount_cd9660 failed");
			}
		} else {
			errx(1, "unknown fstype \"%s\"", fstype);
		}

		free(mp);
	}

	return 2*objsize + 1;
}

struct {
	const char *name;
	int (*handler)(jsmntok_t *, int, const char *);
} parsers[] = {
	{ "cmdline", handle_cmdline },
	{ "env", handle_env },
	{ "blk", handle_blk },
	{ "net", handle_net },
};

void
_rumprun_config(const char *cmdline)
{
	jsmn_parser p;
	jsmntok_t *tokens = NULL;
	jsmntok_t *t;
	size_t cmdline_len = strlen(cmdline);
	int i, ntok;

	while (*cmdline != '{') {
		if (*cmdline == '\0') {
			warnx("could not find start of json.  no config?");
			makeargv("rumprun");
			return;
		}
		cmdline++;
	}

	jsmn_init(&p);
	ntok = jsmn_parse(&p, cmdline, cmdline_len, NULL, 0);

	if (ntok <= 0) {
		errx(1, "command line json parse failed");
	}

	tokens = malloc(ntok * sizeof(*t));
	if (!tokens) {
		errx(1, "failed to allocate jsmn tokens");
	}

	jsmn_init(&p);
	if ((ntok = jsmn_parse(&p, cmdline, cmdline_len, tokens, ntok)) < 1) {
		errx(1, "command line json parse failed");
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

void
_rumprun_deconfig(void)
{

	return; /* TODO */
}
