#define UART0		0x16000000	/* uart address */

#define UARTDR		0x0000		/* data register */
#define UARTFR		0x0018		/* flag register */

#define UARTFR_TXFE	0x80		/* fifo empty */


#define TMR1		0x13000100	/* timer base address */
#define TMR2		0x13000200	/* timer base address */

#define TMR1_LOAD	TMR1 + 0x00	/* load value to decrement */
#define TMR1_VALUE	TMR1 + 0x04	/* current value */
#define TMR1_CTRL	TMR1 + 0x08	/* control register */
#define TMR1_CLRINT	TMR1 + 0x0c	/* clear interrupt by writing here */

#define TMR2_LOAD	TMR2 + 0x00	/* load value to decrement */
#define TMR2_VALUE	TMR2 + 0x04	/* current value */
#define TMR2_CTRL	TMR2 + 0x08	/* control register */
#define TMR2_CLRINT	TMR2 + 0x0c	/* clear interrupt by writing here */

#define TMR_CTRL_EN	0x80		/* timer enabled */
#define TMR_CTRL_PER	0x40		/* periodic (loop to value) */
#define TMR_CTRL_IE	0x20		/* interrupt enable */
#define TMR_CTRL_D256	0x08		/* divide clock by 256 */
#define TMR_CTRL_D16	0x04		/* divide clock by 16 */
#define TMR_CTRL_32	0x02		/* 16/32bit timer */


#define INTR_BASE	0x14000000	/* irq control base */

#define INTR_STATUS	INTR_BASE + 0x0	/* current status */
#define INTR_ENABLE	INTR_BASE + 0x8	/* enable irqs */
#define INTR_CLEAR	INTR_BASE + 0xc	/* clear irqs */

#define CM_SDRAM	0x10000020	/* sdram control register */
