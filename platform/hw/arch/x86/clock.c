/*-
 * Copyright (c) 2014, 2015 Antti Kantee.  All Rights Reserved.
 * Copyright (c) 2015 Martin Lucina.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <bmk/kernel.h>
#include <bmk/clock_subr.h>

#define NSEC_PER_SEC	1000000000ULL
/* Minimum delta to sleep using PIT. Programming seems to have an overhead of
 * 3-4us, but play it safe here. */
#define PIT_MIN_DELTA	16

/* clock isr trampoline (in locore.S) */
void bmk_cpu_isr_clock(void);

/* Multiplier for converting TSC ticks to nsecs. (0.32) fixed point. */
static uint32_t tsc_mult;

/*
 * Multiplier for converting nsecs to PIT ticks. (1.32) fixed point.
 *
 * Calculated as:
 *
 *     f = NSEC_PER_SEC / TIMER_HZ   (0.31) fixed point.
 *     pit_mult = 1 / f              (1.32) fixed point.
 */
static const uint32_t pit_mult = (1ULL << 63) / ((NSEC_PER_SEC << 31) / TIMER_HZ);

/* Base time values at the last call to bmk_cpu_clock_now(). */
static bmk_time_t time_base;
static uint64_t tsc_base;

/* RTC wall time offset at monotonic time base. */
static bmk_time_t rtc_epochoffset;

/*
 * Calculate prod = (a * b) where a is (64.0) fixed point and b is (0.32) fixed
 * point.  The intermediate product is (64.32) fixed point, discarding the
 * fractional bits leaves us with a (64.0) fixed point result.
 *
 * XXX Document what range of (a, b) is safe from overflow in this calculation.
 */
static inline uint64_t mul64_32(uint64_t a, uint32_t b)
{
	uint64_t prod;
#if defined(__x86_64__)
	/* For x86_64 the computation can be done using 64-bit multiply and
	 * shift. */
	__asm__ (
		"mul %%rdx ; "
		"shrd $32, %%rdx, %%rax"
		: "=a" (prod)
		: "0" (a), "d" ((uint64_t)b)
	);
#elif defined(__i386__)
	/* For i386 we compute the partial products and add them up, discarding
	 * the lower 32 bits of the product in the process. */
	uint32_t h = (uint32_t)(a >> 32);
	uint32_t l = (uint32_t)a;
	uint32_t t1, t2;
	__asm__ (
		"mul  %5       ; "  /* %edx:%eax = (l * b)                    */
		"mov  %4,%%eax ; "  /* %eax = h                               */
		"mov  %%edx,%4 ; "  /* t1 = ((l * b) >> 32)                   */
		"mul  %5       ; "  /* %edx:%eax = (h * b)                    */
		"add  %4,%%eax ; "
		"xor  %5,%5    ; "
		"adc  %5,%%edx ; "  /* %edx:%eax = (h * b) + ((l * b) >> 32)  */
		: "=A" (prod), "=r" (t1), "=r" (t2)
		: "a" (l), "1" (h), "2" (b)
	);
#else
#error
#endif

	return prod;
}

/*
 * Read the current i8254 channel 0 tick count.
 */
static unsigned int
i8254_gettick(void)
{
	uint16_t rdval;

	outb(TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);
	rdval = inb(TIMER_CNTR);
	rdval |= (inb(TIMER_CNTR) << 8);
	return rdval;
}

/*
 * Delay for approximately n microseconds using the i8254 channel 0 counter.
 * Timer must be programmed appropriately before calling this function.
 */
static void
i8254_delay(unsigned int n)
{
	unsigned int cur_tick, initial_tick;
	int remaining;
	const unsigned long timer_rval = TIMER_HZ / 100;

	initial_tick = i8254_gettick();

	remaining = (unsigned long long) n * TIMER_HZ / 1000000;

	while (remaining > 1) {
		cur_tick = i8254_gettick();
		if (cur_tick > initial_tick)
			remaining -= timer_rval - (cur_tick - initial_tick);
		else
			remaining -= initial_tick - cur_tick;
		initial_tick = cur_tick;
	}
}

/*
 * Read a RTC register. Due to PC platform braindead-ness also disables NMI.
 */
static inline uint8_t rtc_read(uint8_t reg)
{

	outb(RTC_COMMAND, reg | RTC_NMI_DISABLE);
	return inb(RTC_DATA);
}

/*
 * Return current RTC time. Note that due to waiting for the update cycle to
 * complete, this call may take some time.
 */
static bmk_time_t rtc_gettimeofday(void)
{
	struct bmk_clock_ymdhms dt;

	splhigh();

	/* If time update in progress then spin until complete. */
	while(rtc_read(RTC_STATUS_A) & RTC_UIP)
		continue;

	dt.dt_sec = bcdtobin(rtc_read(RTC_SEC));
	dt.dt_min = bcdtobin(rtc_read(RTC_MIN));
	dt.dt_hour = bcdtobin(rtc_read(RTC_HOUR));
	dt.dt_day = bcdtobin(rtc_read(RTC_DAY));
	dt.dt_mon = bcdtobin(rtc_read(RTC_MONTH));
	dt.dt_year = bcdtobin(rtc_read(RTC_YEAR)) + 2000;

	spl0();

	return bmk_clock_ymdhms_to_secs(&dt) * NSEC_PER_SEC;
}

void
bmk_x86_initclocks(void)
{
	uint64_t tsc_freq;

	/* Initialise i8254 timer channel 0 to mode 2 at 100 Hz */
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
	outb(TIMER_CNTR, (TIMER_HZ / 100) & 0xff);
	outb(TIMER_CNTR, (TIMER_HZ / 100) >> 8);

	/*
	 * Read RTC time to use as epoch offset. This must be done just before
	 * tsc_base is initialised in order to get a correct offset.
	 */
	rtc_epochoffset = rtc_gettimeofday();

	/*
	 * Calculate TSC frequency by calibrating against an 0.1s delay
	 * using the i8254 timer.
	 */
	spl0();
	tsc_base = rdtsc();
	i8254_delay(100000);
	tsc_freq = (rdtsc() - tsc_base) * 10;
	splhigh();

	/*
	 * Reinitialise i8254 timer channel 0 to mode 4 (one shot).
	 */
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_ONESHOT | TIMER_16BIT);

	/*
	 * Calculate TSC scaling multiplier.
	 *
	 * (0.32) tsc_mult = NSEC_PER_SEC (32.32) / tsc_freq (32.0)
	 */
	tsc_mult = (NSEC_PER_SEC << 32) / tsc_freq;

	/*
	 * Monotonic time begins at tsc_base (first read of TSC before
	 * calibration).
	 */
	time_base = mul64_32(tsc_base, tsc_mult);

	/*
	 * Map i8254 interrupt vector and enable it in the PIC.
	 * XXX: We don't really want to enable IRQ2 here, but ...
	 */
	bmk_x86_fillgate(32, bmk_cpu_isr_clock, 0);
	outb(PIC1_DATA, 0xff & ~(1<<2|1<<0));
}

void
bmk_isr_clock(void)
{

	/*
	 * Nothing to do here, the clock interrupt serves only as a way to wake
	 * the CPU from halt state.
	 */
}

/*
 * Return monotonic time since system boot in nanoseconds.
 */
bmk_time_t
bmk_cpu_clock_now(void)
{
	uint64_t tsc_now, tsc_delta;

	splhigh();

	/*
	 * Update time_base (monotonic time) and tsc_base (TSC time).
	 */
	tsc_now = rdtsc();
	tsc_delta = tsc_now - tsc_base;
	time_base += mul64_32(tsc_delta, tsc_mult);
	tsc_base = tsc_now;

	spl0();

	return time_base;
}

/*
 * Return epoch offset (wall time offset to monotonic clock start).
 */
bmk_time_t
bmk_cpu_clock_epochoffset(void)
{

	return rtc_epochoffset;
}

/*
 * Block the CPU until monotonic time is *no later than* the specified time.
 * Returns early if any interrupts are serviced, or if the requested delay is
 * too short.
 */
void
bmk_cpu_block(bmk_time_t until)
{
	bmk_time_t now, delta_ns;
	int64_t delta_ticks;
	unsigned int ticks;

	/*
	 * Return if called too late.
	 */
	now = bmk_cpu_clock_now();
	if (until < now)
		return;

	/*
	 * Compute delta in PIT ticks. Return if it is less than minimum safe
	 * amount of ticks.
	 */
	delta_ns = until - now;
	delta_ticks = mul64_32(delta_ns, pit_mult);
	if (delta_ticks < PIT_MIN_DELTA)
		return;

	/*
	 * Program the timer to interrupt the CPU after the delay has expired.
	 * Maximum timer delay is 65535 ticks.
	 */
	if (delta_ticks > 65535)
		ticks = 65535;
	else
		ticks = delta_ticks;

	/*
	 * Note that according to the Intel 82C54 datasheet, p12 the
	 * interrupt is actually delivered in N + 1 ticks.
	 */
	outb(TIMER_CNTR, (ticks - 1) & 0xff);
	outb(TIMER_CNTR, (ticks - 1) >> 8);

	/*
	 * Wait for any interrupt. If we got an interrupt then
	 * just return into the scheduler which will check if there is
	 * work to do and send us back here if not.
	 *
	 * TODO: It would be more efficient for longer sleeps to be
	 * able to distinguish if the interrupt was the PIT interrupt
	 * and no other, but this will do for now.
	 */
	hlt();
}
