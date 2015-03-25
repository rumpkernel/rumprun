/*
 * The assumption is that these won't be used very often,
 * only for the very low-level routines.
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
