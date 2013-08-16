#include <mini-os/types.h>
#include <mini-os/console.h>
#include <mini-os/netfront.h>
#include <mini-os/xmalloc.h>

/* some hacks to satisfy type requirements.  XXX: fix me */
typedef long register_t;
typedef uint64_t dev_t;
typedef int mode_t;
typedef int pid_t;
typedef int uid_t;
typedef int gid_t;
typedef int socklen_t;
typedef int sigset_t;
typedef struct {
	int xxx;
} fd_set;

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
	struct rump_ufs_args ua;
	struct rump_dirent *dp;
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

/* XXX: simply copypaste/mangle for now */
struct sockaddr_in {
	uint8_t		sin_len;
	uint8_t		sin_family;
	uint16_t	sin_port;
	uint32_t	sin_addr;
	int8_t		sin_zero[8];
};

static void
donet(void)
{
	struct sockaddr_in sin;
	char buf[128];
	int s, s2;
	int rv;
	
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
		printk("no socket\n");
		return;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = RUMP_AF_INET;
	sin.sin_port = 0x0010; /* 0x1000 on LE */
	sin.sin_addr = 0;
	if (rump_sys_bind(s, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
		printk("bind fail\n");
		return;
	}
	if (rump_sys_listen(s, 10) == -1) {
		printk("unix man, please listen()\n");
		return;
	}

	printk("listening for connections on port 4096 (16 if you're BE)\n");

	for (;;) {
		int slen = sizeof(sin);
		char *p;
		size_t bpos;
		ssize_t nn;

		s2 = rump_sys_accept(s, (struct sockaddr *)&sin, &slen);
		if (s2 == -1)
			continue;

#define PROMPT "LOGON: "
#define GREET "GREETINGS PROFESSOR FALKEN.\n"
#define NOPE "LOGIN INCORRECT\n"
		rump_sys_write(s2, PROMPT, sizeof(PROMPT)-1);

		memset(buf, 0, sizeof(buf));
		bpos = 0;
		while (!(p = strchr(buf, '\n')) && bpos < sizeof(buf)) {
			nn = rump_sys_read(s2, buf+bpos, sizeof(buf)-bpos);
			if (nn <= 0) {
				break;
			}
			bpos += nn;
		}
		if (!p) {
			rump_sys_close(s2);
			continue;
		}
		*p = '\0';
		/* multiple holes here, some more microsofty than others */
		if (strncmp(buf, "Joshua", 6) == 0) {
			rump_sys_write(s2, GREET, sizeof(GREET)-1);
		} else if (strncmp(buf, "reboot", 6) == 0) {
			return;
		} else {
			rump_sys_write(s2, NOPE, sizeof(NOPE)-1);
		}
		msleep(5000);
		rump_sys_close(s2);
	}
}

void
demo_thread(void *arg)
{

	rump_init();

	if (1)
		dofs();
	schedule();
	if (1)
		donet();

	rump_sys_reboot(0, 0);
}
