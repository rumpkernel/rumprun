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
