/*-
 * Copyright (c) 2014 Antti Kantee.  All Rights Reserved.
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

#include <sys/types.h>

#include <stdio.h>

#include <rump/rump.h>
#include <rump/netconfig.h>

#include <bmk/app.h>

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

	rumpkern_config();

	printf("=== calling main() ===\n\n");
        rv = main(1, argv);
	printf("=== main() returned %d ===\n\n", rv);

	/* XXX: just fall somewhere */
}
