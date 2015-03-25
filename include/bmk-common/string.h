#ifndef _BMK_STRING_H_
#define _BMK_STRING_H_

void *bmk_memcpy(void *, const void *, unsigned long);
void *bmk_memset(void *, int, unsigned long);

unsigned long bmk_strlen(const char *);
char *bmk_strcpy(char *, const char *);
char *bmk_strncpy(char *, const char *, unsigned long);
int bmk_strcmp(const char *, const char *);

#endif /* _BMK_STRING_H_ */
