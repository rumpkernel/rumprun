#include <mini-os/console.h>
#include <mini-os/ctype.h>

#include <limits.h>

#define _FUNCNAME strtoul
#define __UINT unsigned long
#define __UINT_MAX ULONG_MAX

#define _STANDALONE
#define _DIAGASSERT(a)

#include "_strtoul.h"
