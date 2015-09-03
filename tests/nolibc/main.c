#include <hw/kernel.h>

#include <bmk-core/errno.h>
#include <bmk-core/printf.h>
#include <bmk-core/string.h>

#include <rump/rump.h>

#include "nolibc.h"

static ssize_t
writestr(int fd, const char *str)
{
	return rump_sys_write(fd, str, bmk_strlen(str));
}

void
mainthread(void *cmdline)
{
	int rv, fd;

	rv = rump_init();
	bmk_printf("rump kernel init complete, rv %d\n", rv);

	writestr(1, "Hello, stdout!\n");

	bmk_printf("open(/notexisting): ");
	fd = rump_sys_open("/notexisting", 0);
	if (fd == -1) {
		int errno = *bmk_sched_geterrno();
		if (errno == RUMP_ENOENT) {
			bmk_printf("No such file or directory. All is well.\n");
		} else {
			bmk_printf("Something went wrong. errno = %d\n", errno);
		}
	} else {
		bmk_printf("Success?! fd=%d\n", fd);
	}

	rump_sys_reboot(0, NULL);
}
