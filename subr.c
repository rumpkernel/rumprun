/*
 * Fee fi fo fum, I smell the blood of a buggy one
 * (you gotta do what you gotta do when living on bare metal)
 */

#include <bmk/types.h>
#include <bmk/kernel.h>
#include <bmk/string.h>

size_t
strlen(const char* str)
{
	size_t rv = 0;

	while (*str++)
		rv++;
	return rv;
}

int
strcmp(const char *a, const char *b)
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
strcpy(char *d, const char *s)
{
	char *orig = d;

	while ((*d++ = *s++) != '\0')
		continue;
	return orig;
}

char *
strncpy(char *d, const char *s, size_t n)
{
	char *orig = d;

	while ((*d++ = *s++) && n--)
		continue;
	while (n--)
		*d++ = '\0';
	return orig;
}

void *
memset(void *b, int c, size_t n)
{
	uint8_t *v = b;

	while (n--)
		*v++ = (uint8_t)c;

	return b;
}

void *
memcpy(void *d, const void *src, size_t n)
{
	uint8_t *dp;
	const uint8_t *sp;

	dp = d;
	sp = src;

	while (n--)
		*dp++ = *sp++;

	return d;
}
