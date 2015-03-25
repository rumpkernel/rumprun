/*
 * The assumption is that these won't be used very often,
 * only for the very low-level routines.
 *
 * Some code from public domain implementations.
 */

#include <bmk-common/string.h>

unsigned long
bmk_strlen(const char *str)
{
	unsigned long rv = 0;

	while (*str++)
		rv++;
	return rv;
}

int
bmk_strcmp(const char *a, const char *b)
{

	while (*a && *a++ == *b++) {
		continue;
	}
	if (*a) {
		a--;
		b--;
	}
	return *a - *b;
}

int
bmk_strncmp(const char *a, const char *b, unsigned long n)
{
	unsigned char u1, u2;

	while (n-- > 0) {
		u1 = (unsigned char)*a++;
		u2 = (unsigned char)*b++;
		if (u1 != u2)
			return u1 - u2;
		if (u1 == '\0')
			return 0;
	}
	return 0;
}

char *
bmk_strcpy(char *d, const char *s)
{
	char *orig = d;

	while ((*d++ = *s++) != '\0')
		continue;
	return orig;
}

char *
bmk_strncpy(char *d, const char *s, unsigned long n)
{
	char *orig = d;

	while ((*d++ = *s++) && n--)
		continue;
	while (n--)
		*d++ = '\0';
	return orig;
}

void *
bmk_memset(void *b, int c, unsigned long n)
{
	unsigned char *v = b;

	while (n--)
		*v++ = (unsigned char)c;

	return b;
}

void *
bmk_memcpy(void *d, const void *src, unsigned long n)
{
	unsigned char *dp;
	const unsigned char *sp;

	dp = d;
	sp = src;

	while (n--)
		*dp++ = *sp++;

	return d;
}
