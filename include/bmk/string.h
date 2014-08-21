#ifndef _BMK_STRING_H_
#define _BMK_STRING_H_

void *bmk_memcpy(void *, const void *, size_t);
void *bmk_memset(void *, int, size_t);

size_t bmk_strlen(const char *);
char *bmk_strcpy(char *, const char *);
char *bmk_strncpy(char *, const char *, size_t);
int bmk_strcmp(const char *, const char *);

#endif /* _BMK_STRING_H_ */
