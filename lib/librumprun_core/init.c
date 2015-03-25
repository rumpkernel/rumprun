#include <bmk-core/bmk_ops.h>
#include <bmk-core/core.h>

long bmk_stacksize;
long bmk_pagesize;
const struct bmk_ops *bmk_ops;

int
bmk_core_init(long stacksize, long pagesize, const struct bmk_ops *bops)
{

	bmk_stacksize = stacksize;
	bmk_pagesize = pagesize;
	bmk_ops = bops;
}
