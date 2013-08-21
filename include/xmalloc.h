#ifndef __XMALLOC_H__
#define __XMALLOC_H__

#ifdef HAVE_LIBC

#include <stdlib.h>
#include <malloc.h>
/* Allocate space for typed object. */
#define _xmalloc(size, align) memalign(align, size)
#define xfree(ptr) free(ptr)

#else

#include <limits.h>

void *memalloc(size_t, size_t);
void memfree(void *);

#define DEFAULT_ALIGN (sizeof(unsigned long))
#define malloc(size) memalloc(size, DEFAULT_ALIGN)
#define _xmalloc(size, align) memalloc(size, align)
#define free(ptr) memfree(ptr)
#define xfree(ptr) memfree(ptr)

#endif

static inline void *_xmalloc_array(size_t size, size_t align, size_t num)
{
	/* Check for overflow. */
	if (size && num > UINT_MAX / size)
		return NULL;
 	return _xmalloc(size * num, align);
}

/* Allocate space for typed object. */
#define xmalloc(_type) ((_type *)_xmalloc(sizeof(_type), __alignof__(_type)))

/* Allocate space for array of typed objects. */
#define xmalloc_array(_type, _num) ((_type *)_xmalloc_array(sizeof(_type), __alignof__(_type), _num))

#endif /* __XMALLOC_H__ */
