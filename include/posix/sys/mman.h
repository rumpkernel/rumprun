#ifndef _POSIX_SYS_MMAN_H
#define _POSIX_SYS_MMAN_H

#define PROT_READ	0x1
#define PROT_WRITE	0x2
#define PROT_EXEC	0x4

#define MAP_SHARED	0x01
#define MAP_PRIVATE	0x02
#define MAP_ANON	0x20

/* Pages are always resident anyway */
#define MAP_LOCKED	0x0

#define MAP_FAILED	((void*)0)

void *mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset) asm("mmap64");
int munmap(void *start, size_t length);
static inline mlock(const void *addr, size_t len) { return 0; }
static inline munlock(const void *addr, size_t len) { return 0; }

#endif /* _POSIX_SYS_MMAN_H */
