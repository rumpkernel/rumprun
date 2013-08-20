#include <posix/limits.h>
#include <mini-os/console.h>
#include <mini-os/ctype.h>

#define _FUNCNAME strtoul
#define __UINT unsigned long
#define __UINT_MAX ULONG_MAX

#define _STANDALONE
#define _DIAGASSERT(a)

#define __UNCONST(a) (void *)(uintptr_t)(a)

#include "_strtoul.h"
