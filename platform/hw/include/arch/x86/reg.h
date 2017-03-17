#define CPUID_01H_LEAF	0x01

#define CPUID_01H_EDX_SSE	0x02000000 /* SSE Extensions */

#define CR0_PG		0x80000000 /* Paging */
#define CR0_WP		0x00010000 /* Write Protect */
#define CR0_PE		0x00000001 /* Protection Enable */

#define CR4_OSXMMEXCPT	0x00000400 /* OS support for unmasked SIMD FP exceptions */
#define CR4_OSFXSR	0x00000200 /* OS support for FXSAVE & FXRSTOR */
#define CR4_PAE		0x00000020 /* Physical Address Extension */

/* Extended Feature Enable Register */
#define MSR_EFER	0xc0000080

#define MSR_EFER_LME	0x00000100 /* Long Mode Enable */

#define PIC1_CMD	0x20
#define PIC1_DATA	0x21
#define PIC2_CMD	0xa0
#define PIC2_DATA	0xa1
#define ICW1_IC4	0x01	/* we're going to do the fourth write */
#define ICW1_INIT	0x10
#define ICW4_8086	0x01	/* use 8086 mode */

#define TIMER_CNTR	0x40
#define TIMER_MODE	0x43
#define TIMER_SEL0	0x00
#define TIMER_LATCH	0x00
#define TIMER_RATEGEN	0x04
#define TIMER_ONESHOT	0x08
#define TIMER_16BIT	0x30
#define TIMER_HZ	1193182

#define	RTC_COMMAND	0x70
#define	RTC_DATA	0x71
#define RTC_NMI_DISABLE	(1<<8)
#define RTC_NMI_ENABLE	0
#define	RTC_SEC		0x00
#define	RTC_MIN		0x02
#define	RTC_HOUR	0x04
#define	RTC_DAY		0x07
#define	RTC_MONTH	0x08
#define	RTC_YEAR	0x09
#define	RTC_STATUS_A	0x0a
#define	RTC_UIP		(1<<7)

#define CONS_WIDTH	80
#define CONS_HEIGHT	25
#define CONS_ADDRESS	0xb8000

#define COM_DATA	0
#define COM_DLBL	0
#define COM_DLBH	1
#define COM_IER		1
#define COM_FIFO	2
#define COM_LCTL	3
#define COM_LSR		5

#define BIOS_COM1_BASE	0x400
#define BIOS_CRTC_BASE	0x463
