#include <sys/types.h>

#include <stdio.h>

#include <rump/rump.h>
#include <rump/netconfig.h>

#include <bmk/app.h>

#include "netbsd_init.h"

int bmk_havenet;

/*
 * boot and configure rump kernel
 */
static void
rumpkern_config(void)
{
	int rv = 1;

	/* le hack */
	if (rump_pub_netconfig_ifup("wm0") == 0)
		rv = rump_pub_netconfig_dhcp_ipv4_oneshot("wm0");
	else if (rump_pub_netconfig_ifup("pcn0") == 0)
		rv = rump_pub_netconfig_dhcp_ipv4_oneshot("pcn0");
	else if (rump_pub_netconfig_ifup("vioif0") == 0)
		rv = rump_pub_netconfig_dhcp_ipv4_oneshot("vioif0");
	bmk_havenet = rv == 0;
}

int __nopmain(void);
int
__nopmain(void)
{

	printf("THIS IS NOT MAIN\n");
	return 0;
}
__weak_alias(main,__nopmain);

void
bmk_beforemain(void)
{
        char *argv[] = {"bmk_main", 0};
	int rv;

	rump_init();
	_netbsd_init();
	rumpkern_config();

	printf("=== calling main() ===\n\n");
        rv = main(1, argv);
	printf("=== main() returned %d ===\n\n", rv);

	_netbsd_fini();

	/* XXX: just fall somewhere */
}
