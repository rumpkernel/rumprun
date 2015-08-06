static inline uint8_t
inb(unsigned long address)
{

	return *(volatile uint8_t *)address;
}

static inline uint32_t
inl(unsigned long address)
{

	return *(volatile uint32_t *)address;
}

static inline void
outb(unsigned long address, uint8_t value)
{

	*(volatile uint8_t *)address = value;
}

static inline void
outl(unsigned long address, uint32_t value)
{

	*(volatile uint32_t *)address = value;
}
