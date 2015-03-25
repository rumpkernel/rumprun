#include <stdlib.h>
#include <string.h>

#include <bmk-core/errno.h>
#include <bmk-core/memalloc.h>

int
posix_memalign(void **rv, size_t nbytes, size_t align)
{
	void *v;
	int error = BMK_ENOMEM;

	if ((v = bmk_memalloc(nbytes, align)) != NULL) {
		*rv = v;
		error = 0;
	}

	return error;
}

void *
malloc(size_t size)
{

	return bmk_memalloc(size, 8);
}

void *
calloc(size_t n, size_t size)
{

	return bmk_memcalloc(n, size);
}

void *
realloc(void *cp, size_t nbytes)
{

	return bmk_memrealloc(cp, nbytes);
}

void
free(void *cp)
{

	bmk_memfree(cp);
}
