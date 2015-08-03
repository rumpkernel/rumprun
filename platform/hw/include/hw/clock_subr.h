/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _BMK_CLOCK_SUBR_H_
#define _BMK_CLOCK_SUBR_H_

#include <hw/kernel.h>

/* Some handy constants. */
#define SECS_PER_MINUTE		60
#define SECS_PER_HOUR		3600
#define SECS_PER_DAY		86400
#define DAYS_PER_COMMON_YEAR    365
#define DAYS_PER_LEAP_YEAR      366
#define SECS_PER_COMMON_YEAR	(SECS_PER_DAY * DAYS_PER_COMMON_YEAR)
#define SECS_PER_LEAP_YEAR	(SECS_PER_DAY * DAYS_PER_LEAP_YEAR)

/* Traditional POSIX base year */
#define	POSIX_BASE_YEAR	1970

/* Some handy functions */
static inline int
days_in_month(int m)
{
	switch (m) {
	case 2:
		return 28;
	case 4: case 6: case 9: case 11:
		return 30;
	case 1: case 3: case 5: case 7: case 8: case 10: case 12:
		return 31;
	default:
		return -1;
	}
}

/*
 * This inline avoids some unnecessary modulo operations
 * as compared with the usual macro:
 *   ( ((year % 4) == 0 &&
 *      (year % 100) != 0) ||
 *     ((year % 400) == 0) )
 * It is otherwise equivalent.
 */
static inline int
is_leap_year(uint64_t year)
{
	if ((year & 3) != 0)
		return 0;

	if ((year % 100) != 0)
		return 1;

	return (year % 400) == 0;
}

static inline int
days_per_year(uint64_t year)
{
	return is_leap_year(year) ? DAYS_PER_LEAP_YEAR : DAYS_PER_COMMON_YEAR;
}

/*
 * "POSIX time" to/from "YY/MM/DD/hh/mm/ss"
 */
struct bmk_clock_ymdhms {
	uint64_t dt_year;
	uint8_t dt_mon;
	uint8_t dt_day;
	uint8_t dt_hour;
	uint8_t dt_min;
	uint8_t dt_sec;
};

bmk_time_t bmk_clock_ymdhms_to_secs(struct bmk_clock_ymdhms *);

/*
 * BCD to binary.
 */
static inline unsigned int
bcdtobin(unsigned int bcd)
{
        return ((bcd >> 4) & 0x0f) * 10 + (bcd & 0x0f);
}

#endif /* _BMK_CLOCK_SUBR_H_ */
