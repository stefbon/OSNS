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

#ifndef LIB_SYSTEM_TIME_H
#define LIB_SYSTEM_TIME_H

typedef int64_t			system_time_sec_t;
typedef uint32_t		system_time_nsec_t;

struct system_timespec_s {
    system_time_sec_t		st_sec;
    system_time_nsec_t		st_nsec;
};

#define SYSTEM_TIME_INIT	{0, 0}

#define SYSTEM_TIME_ADD_DECI	1
#define SYSTEM_TIME_ADD_CENTI	2
#define SYSTEM_TIME_ADD_MILLI	3
#define SYSTEM_TIME_ADD_MICRO	4
#define SYSTEM_TIME_ADD_NANO	5
#define SYSTEM_TIME_ADD_ZERO	6
#define SYSTEM_TIME_ADD_DECA	7
#define SYSTEM_TIME_ADD_HECTO	8
#define SYSTEM_TIME_ADD_KILO	9
#define SYSTEM_TIME_ADD_MEGA	10
#define SYSTEM_TIME_ADD_GIGA	11


/* Prototypes */

void get_current_time_system_time(struct system_timespec_s *time);

void system_time_add_time(struct system_timespec_s *expire, struct system_timespec_s *plus);
void system_time_substract_time(struct system_timespec_s *expire, struct system_timespec_s *minus);

void set_expire_time_system_time(struct system_timespec_s *expire, struct system_timespec_s *timeout);
void get_expired_time_system_time(struct system_timespec_s *time, struct system_timespec_s *expired);
void system_time_add(struct system_timespec_s *time, unsigned char what, uint32_t count);
void copy_system_time(struct system_timespec_s *to, struct system_timespec_s *from);
int system_time_test_earlier(struct system_timespec_s *a, struct system_timespec_s *b);

void convert_system_time_from_double(struct system_timespec_s *time, double from);
double convert_system_time_to_double(struct system_timespec_s *time);

void set_system_time(struct system_timespec_s *time, system_time_sec_t sec, system_time_nsec_t nsec);

system_time_sec_t get_system_time_sec(struct system_timespec_s *time);
system_time_nsec_t get_system_time_nsec(struct system_timespec_s *time);

#endif
