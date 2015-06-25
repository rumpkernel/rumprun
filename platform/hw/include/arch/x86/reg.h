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
