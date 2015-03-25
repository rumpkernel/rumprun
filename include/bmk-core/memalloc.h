#ifndef _BMK_MEMALLOC_H_
#define _BMK_MEMALLOC_H_

void *  bmk_memalloc(unsigned long, unsigned long);
void *  bmk_memrealloc(void *, unsigned long);
void *  bmk_xmalloc(unsigned long);
void    bmk_memfree(void *);

#endif /* _BMK_MEMALLOC_H_ */
