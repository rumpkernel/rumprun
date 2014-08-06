#ifndef _BMK_STRING_H_
#define _BMK_STRING_H_

#include <bmk/types.h>

void *memcpy(void *, const void *, size_t);
void *memset(void *, int, size_t);

size_t strlen(const char *);
char *strcpy(char *, const char *);
char *strncpy(char *, const char *, size_t);
int strcmp(const char *, const char *);

#endif /* _BMK_STRING_H_ */
