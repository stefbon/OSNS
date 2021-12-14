/*

  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#include <inttypes.h>
#include <ctype.h>

#include <sys/stat.h>
#include <sys/param.h>
#include <sys/types.h>
#include <time.h>

#include "time.h"
#include "logging.h"

#define SYSTEM_TIME_NSEC_MAX	1000000000

void get_current_time_system_time(struct system_timespec_s *time)
{

#ifdef __linux__

    struct timespec tmp;
    int res=clock_gettime(CLOCK_REALTIME, &tmp);

    time->st_sec=tmp.tv_sec;
    time->st_nsec=tmp.tv_nsec;

#else

    time->st_sec=0;
    time->st_nsec=0;

#endif

}

void system_time_add_time(struct system_timespec_s *expire, struct system_timespec_s *plus)
{

    expire->st_sec+=plus->st_sec;
    expire->st_nsec+=plus->st_nsec;

    if (expire->st_nsec > SYSTEM_TIME_NSEC_MAX) {

	expire->st_nsec -= SYSTEM_TIME_NSEC_MAX;
	expire->st_sec++;

    }

}

void system_time_substract_time(struct system_timespec_s *expire, struct system_timespec_s *minus)
{

    expire->st_sec-=minus->st_sec;

    if (expire->st_nsec < minus->st_nsec) {

	expire->st_sec--;
	expire->st_nsec+=(SYSTEM_TIME_NSEC_MAX - minus->st_nsec);

    } else {

	expire->st_nsec-=minus->st_nsec;

    }

}

void set_expire_time_system_time(struct system_timespec_s *expire, struct system_timespec_s *timeout)
{

    get_current_time_system_time(expire);
    system_time_add_time(expire, timeout);

}

/* get the expired time when comparing time to current time (==now)
    two cases:
    - time is in the past -> expired seconds is positive
    - time is in the future -> expired seconds negative
    note the nsec is by definition always positive ... */

void get_expired_time_system_time(struct system_timespec_s *time, struct system_timespec_s *expired)
{
    struct system_timespec_s tmp;

    get_current_time_system_time(&tmp);
    expired->st_sec=(tmp.st_sec - time->st_sec);

    if (time->st_nsec <= tmp.st_nsec) {

	/* nsec time is in the past or present */

	expired->st_nsec=tmp.st_nsec - time->st_nsec;

    } else {

	expired->st_sec--;
	expired->st_nsec=time->st_nsec - tmp.st_nsec;

    }

}

void copy_system_time(struct system_timespec_s *to, struct system_timespec_s *from)
{
    memcpy(to, from, sizeof(struct system_timespec_s));
}

static void correct_nsec_check_bound(struct system_timespec_s *time, uint32_t base, uint32_t count)
{

    uint32_t sec= ((time->st_nsec + (base * count)) / SYSTEM_TIME_NSEC_MAX);
    uint32_t nsec= ((time->st_nsec + (base * count)) % SYSTEM_TIME_NSEC_MAX);

    time->st_sec += sec;
    time->st_nsec += nsec;

}

void system_time_add(struct system_timespec_s *time, unsigned char what, uint32_t count)
{

    switch (what) {

	case SYSTEM_TIME_ADD_DECI:

	    /* one in ten (= 10^1) */

	    correct_nsec_check_bound(time, 100000000, count);
	    break;

	case SYSTEM_TIME_ADD_CENTI:

	    /* one in hundredth (=10^2) */

	    correct_nsec_check_bound(time, 10000000, count);
	    break;

	case SYSTEM_TIME_ADD_MILLI:

	    /* one in thousand (=10^3) */

	    correct_nsec_check_bound(time, 1000000, count);
	    break;

	case SYSTEM_TIME_ADD_MICRO:

	    /* one in million (=10^6) */

	    correct_nsec_check_bound(time, 1000, count);
	    break;

	case SYSTEM_TIME_ADD_NANO:

	    /*one */

	    correct_nsec_check_bound(time, 1, count);
	    break;

	case SYSTEM_TIME_ADD_ZERO:

	    time->st_sec += count;
	    break;

	case SYSTEM_TIME_ADD_DECA:

	    time->st_sec += (10 * count);
	    break;

	case SYSTEM_TIME_ADD_HECTO:

	    time->st_sec += (100 * count);
	    break;

	case SYSTEM_TIME_ADD_KILO:

	    time->st_sec += (1000 * count);
	    break;

	case SYSTEM_TIME_ADD_MEGA:

	    time->st_sec += (1000000 * count);
	    break;

	case SYSTEM_TIME_ADD_GIGA:

	    time->st_sec += (1000000000 * count);
	    break;

	default:

	    logoutput_error("system_time_add: level %i not supported", what);

    }

    return;


}

int system_time_test_earlier(struct system_timespec_s *a, struct system_timespec_s *b)
{
    int result=0;

    if (a->st_sec > b->st_sec) {

	result=-1;

    } else if (a->st_sec < b->st_sec) {

	result=1;

    } else {

	if (a->st_nsec > b->st_nsec) {

	    result=-1;

	} else if (a->st_nsec < b->st_nsec) {

	    result=1;

	}

    }

    return result;
}

void convert_system_time_from_double(struct system_timespec_s *time, double from)
{
    time->st_sec=(system_time_sec_t) from;
    time->st_nsec=(system_time_nsec_t) ((from - time->st_sec) * SYSTEM_TIME_NSEC_MAX);
}

double convert_system_time_to_double(struct system_timespec_s *time)
{
    double sec=time->st_sec;
    double nsec=time->st_nsec;

    return (sec + (nsec / SYSTEM_TIME_NSEC_MAX));
}

void set_system_time(struct system_timespec_s *time, system_time_sec_t sec, system_time_nsec_t nsec)
{
    time->st_sec=sec;
    time->st_nsec=nsec;
}

system_time_sec_t get_system_time_sec(struct system_timespec_s *time)
{
    return time->st_sec;
}

system_time_nsec_t get_system_time_nsec(struct system_timespec_s *time)
{
    return time->st_nsec;
}
