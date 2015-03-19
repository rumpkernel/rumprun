#include <bmk/types.h>
#include <bmk/multiboot.h>
#include <bmk/kernel.h>
#include <bmk/memalloc.h>
#include <bmk/string.h>
#include <bmk/sched.h>
#include <bmk/app.h>

#include <bmk-common/netbsd_initfini.h>
#include <bmk-common/bmk_ops.h>

unsigned long bmk_membase;
unsigned long bmk_memsize;

/*
 * we don't need freepg
 * (for the humour impaired: it was a joke, on the TODO ... but really,
 * it's not that urgent since the rump kernel uses its own caching
 * allocators, so once the backing pages are allocated, they tend to
 * never get freed)
 */
void *
bmk_allocpg(size_t howmany)
{
	static size_t current = 0;
	unsigned long rv;

	rv = bmk_membase + PAGE_SIZE*current;
	current += howmany;
	if (current*PAGE_SIZE > bmk_memsize)
		return NULL;

	return (void *)rv;
}

static int
parsemem(uint32_t addr, uint32_t len)
{
	struct multiboot_mmap_entry *mbm;
	unsigned long memsize;
	unsigned long ossize, osbegin, osend;
	extern char _end[], _begin[];
	uint32_t off;

	/*
	 * Look for our memory.  We assume it's just in one chunk
	 * starting at MEMSTART.
	 */
	for (off = 0; off < len; off += mbm->size + sizeof(mbm->size)) {
		mbm = (void *)(addr + off);
		if (mbm->addr == MEMSTART
		    && mbm->type == MULTIBOOT_MEMORY_AVAILABLE) {
			break;
		}
	}
	assert(off < len);

	memsize = mbm->len;
	osbegin = (unsigned long)_begin;
	osend = round_page((unsigned long)_end);
	ossize = osend - osbegin;

	bmk_membase = mbm->addr + ossize;
	bmk_memsize = memsize - ossize;

	return 0;
}

#ifdef BMK_APP
static const struct bmk_ops myops = {
	.bmk_halt = bmk_halt,
};
#endif

void
bmk_main(struct multiboot_info *mbi)
{

	bmk_cons_puts("rump kernel bare metal bootstrap\n\n");
	if ((mbi->flags & MULTIBOOT_MEMORY_INFO) == 0) {
		bmk_cons_puts("multiboot memory info not available\n");
		return;
	}
	if (parsemem(mbi->mmap_addr, mbi->mmap_length))
		return;
	bmk_cpu_init();
	bmk_sched_init();
	bmk_isr_init();

#ifdef BMK_APP
	_netbsd_init(BMK_THREAD_STACKSIZE, PAGE_SIZE, &myops);
	bmk_beforemain();
	_netbsd_fini();
#endif
}

/*
 * console.  quick, cheap, dirty, etc.
 * Should eventually keep an in-memory log.  printf-debugging is currently
 * a bit, hmm, limited.
 */
 
#define CONS_WIDTH 80
#define CONS_HEIGHT 25
#define CONS_MAGENTA 0x500
static volatile uint16_t *cons_buf = (volatile uint16_t *)0xb8000;

static void
cons_putat(int c, int x, int y)
{

	cons_buf[x + y*CONS_WIDTH] = CONS_MAGENTA|c;
}

/* display a character in the next available slot */
void
bmk_cons_putc(int c)
{
	static int cons_x;
	static int cons_y;
	int x;
	int doclear = 0;

	if (c == '\n') {
		cons_x = 0;
		cons_y++;
		doclear = 1;
	} else if (c == '\r') {
		cons_x = 0;
	} else {
		cons_putat(c, cons_x++, cons_y);
	}
	if (cons_x == CONS_WIDTH) {
		cons_x = 0;
		cons_y++;
		doclear = 1;
	}
	if (cons_y == CONS_HEIGHT) {
		cons_y--;
		/* scroll screen up one line */
		for (x = 0; x < (CONS_HEIGHT-1)*CONS_WIDTH; x++)
			cons_buf[x] = cons_buf[x+CONS_WIDTH];
	}
	if (doclear) {
		for (x = 0; x < CONS_WIDTH; x++)
			cons_putat(' ', x, cons_y);
	}
}
 
/* display a string */
void
bmk_cons_puts(const char *str)
{

	for (; *str; str++)
		bmk_cons_putc(*str);
}

/*
 * init.  currently just clears the console.
 * rest is done in bmk_main()
 */
void
bmk_init(void)
{
	int x;

	for (x = 0; x < CONS_HEIGHT * CONS_WIDTH; x++)
		cons_putat(' ', x % CONS_WIDTH, x / CONS_WIDTH);
}

void
bmk_halt(void)
{

	bmk_cons_puts("baremetal halted (well, spinning ...)\n");
	for (;;)
		continue;
}
