/* Copyright (c) 2013 Antti Kantee.  See COPYING */

#include <mini-os/console.h>
#include <mini-os/netfront.h>

#include <sys/cdefs.h>
#include <sys/types.h>

#include <netinet/in.h>

#include <ufs/ufs/ufsmount.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rump/rump.h>
#include <rump/netconfig.h>

void demo_thread(void *);

#define BLKDEV(num) "/BLK" __STRING(num)
#define BUFSIZE (64*1024)

static void
dofs(void)
{
	struct ufs_args ua;
	struct dirent *dent;
	DIR *dp;
	void *buf;
	int8_t *p;
	int fd;
	int rv;

	if (open("/not_there", O_RDWR) != -1 || errno != ENOENT)
		errx(1, "errno test");

	if ((rv = rump_pub_etfs_register(BLKDEV(0), "blk0",
	    RUMP_ETFS_BLK)) != 0)
		errx(1, "etfs %s", strerror(rv));

	mkdir("/mnt", 0777);
	ua.fspec = BLKDEV(0);
	if (mount(MOUNT_FFS, "/mnt", 0, &ua, sizeof(ua)) != 0)
		err(1, "mount");

	buf = malloc(BUFSIZE);
	chdir("/mnt");

	dp = opendir(".");
	if (dp == NULL)
		err(1, "opendir");

	printf("\treading directory contents\n");
	while ((dent = readdir(dp)) != NULL) {
		printf("%d %"PRIu64" %s\n",
		    dent->d_type, dent->d_fileno, dent->d_name);
	}
	closedir(dp);

	/* assume README.md exists, open it, and display first line */
	if ((fd = open("README.md", O_RDWR)) == -1)
		err(1, "open README");

	memset(buf, 0, sizeof(buf));
	if (read(fd, buf, 200) < 200)
		err(1, "read");

	if ((p = (void *)strchr(buf, '\n')) == NULL)
		errx(1, "strchr");
	*p = '\0';
	printf("Reading first line of README.md:\n\t===\n%s\n\t===\n\n", (char *)buf);

#define GARBAGE "   TRASHED!   "
	/* write some garbage in there.  spot the difference the next time */
	pwrite(fd, GARBAGE, sizeof(GARBAGE)-1, 20);
	close(fd);

	/*
	 * FS will be unmounted when you type "reboot" into the
	 * net demo, just like in any regular OS
	 */
	sync(); /* but just to be safe */
}

#define MAXCONN 64

struct conn {
	int c_bpos;
	int c_cnt;
	char c_buf[0xe0];
};

static struct pollfd pfds[MAXCONN];
static struct conn conns[MAXCONN];
int maxfd, masterfd;

static void
acceptconn(void)
{
	struct sockaddr_in sin;
	socklen_t slen = sizeof(sin);
	int s;

	if ((s = accept(masterfd, (struct sockaddr *)&sin, &slen)) == -1)
		return;

	/* drop */
	if (s >= MAXCONN) {
		close(s);
		return;
	}

	/* init */
	pfds[s].fd = s;
	memset(&conns[s], 0, sizeof(conns[s]));

	/* XXX: not g/c'd */
	if (s+1 > maxfd)
		maxfd = s+1;

#define PROMPT "LOGON: "
	/* just assume this will go into the socket without blocking */
	write(s, PROMPT, sizeof(PROMPT)-1);
#undef PROMPT
}

static void
readconn(int i)
{
	struct conn *c = &conns[i];
	char *p;
	ssize_t nn;

	nn = read(i, c->c_buf+c->c_bpos, sizeof(c->c_buf)-c->c_bpos);
	/* treat errors and EOF the same way, we shouldn't get EAGAIN */
	if (nn <= 0) {
		close(i);
		pfds[i].fd = -1;
		c->c_cnt = -1;
		return;
	}

	if ((p = strchr(c->c_buf, '\n')) == NULL) {
		c->c_bpos += nn;
		return;
	}

	*p = '\0';
#define GREET "GREETINGS PROFESSOR FALKEN.\n"
#define NOPE "LOGIN INCORRECT\n"
	/* multiple holes here, some more microsofty than others */
	if (strncmp(c->c_buf, "Joshua", 6) == 0) {
		write(i, GREET, sizeof(GREET)-1);
	} else if (strncmp(c->c_buf, "reboot", 6) == 0) {
		reboot(0, 0);
	} else {
		write(i, NOPE, sizeof(NOPE)-1);
	}
	pfds[i].fd = -1;
#undef GREET
#undef NOPE
}

static void
processzombies(void)
{
	int i;

	/*
	 * Let each connection live ~10s regardless of whether it's
	 * completed or not.
	 */
	for (i = 1; i < MAXCONN; i++) {
		if (conns[i].c_cnt != -1 && ++conns[i].c_cnt > 10
                    && pfds[i].fd != -1) {
			close(i);
		}
	}
}

static void
setupnet(void)
{
	int rv;

	if ((rv = rump_pub_netconfig_ifcreate("xenif0")) != 0) {
		printf("creating xenif0 failed: %d\n", rv);
		return;
	}

	/*
	 * Configure the interface using DHCP.  DHCP support is a bit
	 * flimsy, so if this doesn't work properly, you can also use
	 * the manual interface configuration options.
	 */
	if ((rv = rump_pub_netconfig_dhcp_ipv4_oneshot("xenif0")) != 0) {
		printf("getting IP for xenif0 via DHCP failed: %d\n", rv);
		return;
	}
}

static int
sucketonport(uint16_t port)
{
	struct sockaddr_in sin;
	int s;

	s = socket(PF_INET, SOCK_STREAM, 0);
	if (s == -1)
		err(1, "socket");

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = INADDR_ANY;
	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) == -1)
		err(1, "bind");

	if (listen(s, 10) == -1)
		err(1, "unix man, please listen");

	return s;
}

static void
donet(void)
{
	uint64_t zombietime;
	int rv, i;

	setupnet();
	masterfd = sucketonport(4096);
	ASSERT(masterfd < MAXCONN);

	for (i = 0; i < MAXCONN; i++) {
		pfds[i].fd = -1;
		pfds[i].events = POLLIN;
		conns[i].c_cnt = -1;
	}
	pfds[masterfd].fd = masterfd;
	maxfd = masterfd+1;

	printf("WOPR reporting for duty on port 4096\n");

	zombietime = NOW();
	for (;;) {
		if (NOW() - zombietime >= SECONDS(1)) {
			processzombies();
			zombietime = NOW();
		}

		rv = poll(pfds, maxfd, 1000);
		if (rv == 0) {
			printf("still waiting ... %"PRId64"d\n", NOW());
			continue;
		}

		if (rv == -1) {
			printf("fail poll %d\n", errno);
			reboot(0, 0);
		}

		if (pfds[masterfd].revents & POLLIN) {
			acceptconn();
			rv--;
		}

		for (i = 0; i < MAXCONN && rv; i++) {
			if (i == masterfd)
				continue;
			if (pfds[i].fd != -1 && pfds[i].revents & POLLIN) {
				readconn(i);
				rv--;
			}
		}
		ASSERT(rv == 0);
	}
}

int http_main(int, char **);
void *
wwwbozo(void *arg)
{
	char *argv[] = { "bozo", "-X", "/etc" };

	rump_pub_lwproc_switch(arg);

	/*
	 * Call program main.  Since we don't have a new vm space,
	 * ensure that options will be re-parsed.
	 */
	optind = 1;
	optreset = 1;
	httpd_main(sizeof(argv)/sizeof(argv[0]), argv);

	/* among other things, will close fd's */
	rump_pub_lwproc_releaselwp();

	return NULL;
}

static void
dohttpd(void)
{
	struct ufs_args ua;
	struct sockaddr_in sin;
	socklen_t slen;
	int rv, s, sa, count;
	struct lwp *l;
	pthread_t pt;

	if ((rv = rump_pub_etfs_register(BLKDEV(1),
	    "blk1", RUMP_ETFS_BLK)) != 0)
		errx(1, "etfs %s", strerror(rv));

	mkdir("/etc", 0777);
	ua.fspec = BLKDEV(1);
	if (mount(MOUNT_FFS, "/etc", MNT_RDONLY, &ua, sizeof(ua)) == -1)
		err(1, "mount");
	setupnet();

	/* create a decicated process which does the work */
	rump_pub_lwproc_rfork(RUMP_RFCFDG);
	l = rump_pub_lwproc_curlwp();

	/*
	 * ok, now.  we run bozohttpd in inetd mode to gain control
	 * of the worker model is uses.  This means that we have to:
	 *  1: open the listening socket and accept connections
	 *  2: rfork with descriptor inheritance
	 *  3: dup2 the accepted fd's to {0,1,2}
	 *  4: create a mini os thread to handle the request(s)
	 */
	s = sucketonport(80);
	for (count = 0;; count++) {
		/* 1: accept */
		slen = sizeof(sin);
		sa = accept(s, (struct sockaddr *)&sin, &slen);
		if (sa == -1)
			err(1, "accept round %d", count);

		/* 2: rfork */
		rv = rump_pub_lwproc_rfork(RUMP_RFFDG);
		if (rv != 0)
			errx(1, "fork failed: %s", strerror(rv));

		/* 3: setup fd's */
		dup2(sa, 0);
		dup2(sa, 1);
		dup2(sa, 2);
		fcntl(3, F_CLOSEM);

		/* 4: create thread */
		pthread_create(&pt, NULL, wwwbozo, rump_pub_lwproc_curlwp());

		/*
		 * back to handling proc.  yea, this a slightly gray
		 * area of semantics, and we could do a lot better in
		 * preventing a race.  that ssaid, we should be fine due
		 * to cooperative scheduling.
		 *
		 * then, release the reference to the socket fd
		 */
		rump_pub_lwproc_switch(l);
		close(sa);
	}
}

void test_pthread(void);

int
app_main(start_info_t *si)
{
	long tests;

	printf("running demos, command line: %s\n", si->cmd_line);

	if (si->cmd_line[0]) {
		tests = strtol((const char *)si->cmd_line, NULL, 16);
	}

	if (tests & 0x1)
		dofs();
	if (tests & 0x8)
		test_pthread();
	if (tests & 0x2)
		donet();
	if (tests & 0x4)
		dohttpd();

	return 0;
}
