#include <mini-os/console.h>
#include <mini-os/netfront.h>

#include <sys/types.h>

#include <netinet/in.h>

#include <ufs/ufs/ufsmount.h>

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>

#include <rump/rump.h>
#include <rump/rump_syscalls.h>
#include <rump/rumpdefs.h>
#include <rump/netconfig.h>

void demo_thread(void *);

#define BLKDEV "/THEMAGICBLK"
#define BUFSIZE (64*1024)

#define FAIL(a) do { printk(a); return; } while (/*CONSTCOND*/0)

static void
dofs(void)
{
	struct ufs_args ua;
	struct dirent *dp;
	void *buf;
	int8_t *p;
	int fd;
	int rv;

	if ((rv = rump_pub_etfs_register(BLKDEV, "blk0",
	    RUMP_ETFS_BLK)) != 0)
		FAIL("etfs");

	rump_sys_mkdir("/mnt", 0777);
	ua.fspec = BLKDEV;
	if (rump_sys_mount("ffs", "/mnt", 0, &ua, sizeof(ua)) != 0)
		FAIL("mount");

	buf = malloc(BUFSIZE);
	rump_sys_chdir("/mnt");
	if ((fd = rump_sys_open(".", RUMP_O_RDONLY)) == -1)
		FAIL("open");

	if ((rv = rump_sys_getdents(fd, buf, BUFSIZE)) <= 1)
		FAIL("getdents");
	rump_sys_close(fd);

	for (p = buf; p < (int8_t *)buf + rv; p += dp->d_reclen) {
		dp = (void *)p;
		printk("%d %d %s\n", dp->d_type, (int)dp->d_fileno, dp->d_name);
	}

	/* assume README.md exists, open it, and display first line */
	if ((fd = rump_sys_open("README.md", RUMP_O_RDWR)) == -1)
		FAIL("open README");

	memset(buf, 0, sizeof(buf));
	if (rump_sys_read(fd, buf, 200) < 200)
		FAIL("read");

	if ((p = (void *)strchr(buf, '\n')) == NULL)
		FAIL("strchr");
	*p = '\0';
	printk("Reading first line of README.md:\n\t===\n%s\n\t===\n\n", buf);

#define GARBAGE "   TRASHED!   "
	/* write some garbage in there.  spot the difference the next time */
	rump_sys_pwrite(fd, GARBAGE, sizeof(GARBAGE)-1, 20);
	rump_sys_close(fd);

	/*
	 * FS will be unmounted when you type "reboot" into the
	 * net demo, just like in any regular OS
	 */
	rump_sys_sync(); /* but just to be safe */
}

#define MAXCONN 64

struct conn {
	int c_bpos;
	int c_cnt;
	char c_buf[0xe0];
};

static struct pollfd pfds[MAXCONN];
static struct conn conns[MAXCONN];
int maxfd;

static void
acceptconn(void)
{
	struct sockaddr_in sin;
	socklen_t slen = sizeof(sin);
	int s;

	if ((s = rump_sys_accept(0, (struct sockaddr *)&sin, &slen)) == -1)
		return;

	/* drop */
	if (s >= MAXCONN) {
		rump_sys_close(s);
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
	rump_sys_write(s, PROMPT, sizeof(PROMPT)-1);
#undef PROMPT
}

static void
readconn(int i)
{
	struct conn *c = &conns[i];
	char *p;
	ssize_t nn;

	nn = rump_sys_read(i, c->c_buf+c->c_bpos, sizeof(c->c_buf)-c->c_bpos);
	/* treat errors and EOF the same way, we shouldn't get EAGAIN */
	if (nn <= 0) {
		rump_sys_close(i);
		pfds[i].fd = -1;
		c->c_cnt = -1;
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
		rump_sys_write(i, GREET, sizeof(GREET)-1);
	} else if (strncmp(c->c_buf, "reboot", 6) == 0) {
		rump_sys_reboot(0, 0);
	} else {
		rump_sys_write(i, NOPE, sizeof(NOPE)-1);
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
		if (conns[i].c_cnt != -1 && ++conns[i].c_cnt > 10) {
			rump_sys_close(i);
		}
	}
}

static void
donet(void)
{
	struct sockaddr_in sin;
	uint64_t zombietime;
	int rv, i;
	int s;
	
	if ((rv = rump_pub_netconfig_ifcreate("xenif0")) != 0) {
		printk("creating xenif0 failed: %d\n", rv);
		return;
	}

	/*
	 * Configure the interface using DHCP.  DHCP support is a bit
	 * flimsy, so if this doesn't work properly, you can also use
	 * the manual interface configuration options.
	 */
	if ((rv = rump_pub_netconfig_dhcp_ipv4_oneshot("xenif0")) != 0) {
		printk("getting IP for xenif0 via DHCP failed: %d\n", rv);
		return;
	}

	s = rump_sys_socket(RUMP_PF_INET, RUMP_SOCK_STREAM, 0);
	if (s == -1) {
		printk("no socket %d\n", errno);
		return;
	}
	ASSERT(s == 0); /* pseudo-XXX */

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = RUMP_AF_INET;
	sin.sin_port = htons(4096);
	sin.sin_addr.s_addr = INADDR_ANY;
	if (rump_sys_bind(s, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
		printk("bind fail %d\n", errno);
		return;
	}
	if (rump_sys_listen(s, 10) == -1) {
		printk("unix man, please listen(): %d\n", errno);
		return;
	}

	for (i = 0; i < MAXCONN; i++) {
		pfds[i].fd = -1;
		pfds[i].events = POLLIN;
		conns[i].c_cnt = -1;
	}
	pfds[0].fd = 0;
	maxfd = 1;

	printk("WOPR reporting for duty on port 4096 (16 if you're BE)\n");

	zombietime = NOW();
	for (;;) {
		if (NOW() - zombietime >= SECONDS(1)) {
			processzombies();
			zombietime = NOW();
		}

		rv = rump_sys_poll(pfds, maxfd, 1000);
		if (rv == 0) {
			printk("still waiting ... %lld\n", NOW());
			continue;
		}

		if (rv == -1) {
			printk("fail poll %d\n", errno);
			rump_sys_reboot(0, 0);
		}

		if (pfds[0].revents & POLLIN) {
			acceptconn();
			rv--;
		}

		for (i = 1; i < MAXCONN && rv; i++) {
			if (pfds[i].fd != -1 && pfds[i].revents & POLLIN) {
				readconn(i);
				rv--;
			}
		}
		ASSERT(rv == 0);
	}
}

void
demo_thread(void *arg)
{
	start_info_t *si = arg;
	int tests;

	if (si->cmd_line[0]) {
		tests = si->cmd_line[0] - '0';
		if (tests < 0 || tests > 3)
			tests = 0;
	}

	rump_init();

	if (tests & 0x1)
		dofs();
	if (tests & 0x2)
		donet();

	rump_sys_reboot(0, 0);
}
