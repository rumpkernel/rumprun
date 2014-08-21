/*
 * Note, this is application-side code.  We have access to the full
 * set of interfaces offered to NetBSD userland applications.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#include <netinet/in.h>

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <rump/rump.h>
#include <rump/netconfig.h>

/*
 * boot and configure rump kernel
 */
static void
rumpkern_config(void)
{

	rump_init();
	rump_pub_netconfig_dhcp_ipv4_oneshot("wm0");
}

/*
 * could use DNS via libc resolver, but would require us
 * creating /etc/resolv.conf first0
 */
#define DESTHOST "149.20.53.86"

void
bmk_app_main(void)
{
	char buf[512];
	struct sockaddr_in sin;
	char *p;
	int s;

	rumpkern_config();

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
