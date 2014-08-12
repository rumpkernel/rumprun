struct region_descriptor;
void bmk_cpu_lidt(struct region_descriptor *);
void bmk_cpu_lgdt(struct region_descriptor *);

static inline uint8_t
inb(uint16_t port)
{
        uint8_t rv;

        __asm__ __volatile__("inb %1, %0" : "=a"(rv) : "d"(port));

        return rv;
}

static inline uint32_t
inl(uint16_t port)
{
        uint32_t rv;

        __asm__ __volatile__("inl %1, %0" : "=a"(rv) : "d"(port));

        return rv;
}

static inline void
outb(uint16_t port, uint8_t value)
{

        __asm__ __volatile__("outb %0, %1" :: "a"(value), "d"(port));
}

static inline void
outl(uint16_t port, uint32_t value)
{

        __asm__ __volatile__("outl %0, %1" :: "a"(value), "d"(port));
}

static inline void
splhigh(void)
{

	__asm__ __volatile__("cli");
}

static inline void
spl0(void)
{

	__asm__ __volatile__("sti");
}

static inline void
hlt(void)
{

	__asm__ __volatile__("hlt");
}
