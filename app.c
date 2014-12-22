/*
 * Note, this is application-side code.  We have access to the full
 * set of interfaces offered to NetBSD userland applications.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <openssl/md5.h>

#include <rump/rump.h>
#include <rump/netconfig.h>

#include <bmk/app.h>

#include "netbsd_init.h"

static int havenet;

/*
 * boot and configure rump kernel
 */
static void
rumpkern_config(void)
{
	int rv = 1;

	rump_init();

	/* le hack */
	if (rump_pub_netconfig_ifup("wm0") == 0)
		rv = rump_pub_netconfig_dhcp_ipv4_oneshot("wm0");
	else if (rump_pub_netconfig_ifup("pcn0") == 0)
		rv = rump_pub_netconfig_dhcp_ipv4_oneshot("pcn0");
	else if (rump_pub_netconfig_ifup("vioif0") == 0)
		rv = rump_pub_netconfig_dhcp_ipv4_oneshot("vioif0");
	havenet = rv == 0;
}

/*
 * could use DNS via libc resolver, but would require us
 * creating /etc/resolv.conf first0
 */
#define DESTHOST "149.20.53.86"
static void
nettest(void)
{
	char buf[512];
	struct sockaddr_in sin;
	char *p;
	int s;

	s = socket(PF_INET, SOCK_STREAM, 0);
	if (s == -1)
		err(1, "socket open failed");

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(80);
	sin.sin_addr.s_addr = inet_addr(DESTHOST);

	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) == -1)
		err(1, "connect failed");

#define HTTPGET "GET / HTTP/1.0\n\n"
	write(s, HTTPGET, sizeof(HTTPGET)-1);
	if (read(s, buf, sizeof(buf)) < 1)
		err(1, "read failed");

	p = strstr(buf, "Content-Type");
	if (p) {
		p = strchr(p, '\n');
		if (p)
			*p = '\0';
	}

	printf("got response:\n%s\n", buf);
	printf("[omitting rest ...]\n");
}

#define TESTMAGIC "PLEASE WRITE ON THIS IMAGE"
static void
disktest(void)
{
	char buf[512];
	unsigned char md5sum[MD5_DIGEST_LENGTH];
	MD5_CTX md5ctx;
	int fd, i;

	fd = open("/dev/rld0d", O_RDWR);
	/* assume it's not present instead of failing */
	if (fd == -1 && (errno == ENOENT || errno == ENXIO)) {
		printf("no disk device available.  skipping test\n");
		return;
	}

	printf("calculating md5sum of /dev/rld0d\n");

	MD5_Init(&md5ctx);
	while (read(fd, buf, sizeof(buf)) == sizeof(buf))
		MD5_Update(&md5ctx, buf, sizeof(buf));
	MD5_Final(md5sum, &md5ctx);

	for (i = 0; i < sizeof(md5sum); i++) {
		printf("%x", md5sum[i]);
	}
	printf("\n");

	if (pread(fd, buf, sizeof(buf), 0) != sizeof(buf))
		err(1, "read sector 0");

	/* check if writing the test output was requested */
	if (strncmp(buf, TESTMAGIC, sizeof(TESTMAGIC)-1) != 0)
		return;

	printf("writing test results onto image\n");
	memset(buf, 0, sizeof(buf));
	strlcat(buf, "OK\n", sizeof(buf));
	for (i = 0; i < sizeof(md5sum); i++) {
		char tmp[4];
		snprintf(tmp, sizeof(tmp), "%02x", md5sum[i]);
		strlcat(buf, tmp, sizeof(buf));
	}
	strlcat(buf, "\n", sizeof(buf));
	pwrite(fd, buf, sizeof(buf), 0);
}

/*
 * Just a simple demo and/or test.
 */
void
bmk_app_main(void)
{

	rumpkern_config();
	_netbsd_init();

	if (havenet)
		nettest();
	disktest();

	printf("\nTHE END\n");
}
