/*
  2010, 2011, 2012, 2013, 2014, 2015 Stef Bon <stefbon@gmail.com>

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

#ifndef OSNS_RECORD_H
#define OSNS_RECORD_H

struct osns_records_hlpr_s {
    uint32_t					count;
    uint16_t					size;
    char					data[];
};

struct osns_record_hlpr_s {
    uint16_t					size;
    char					data[];
};


/* prototypes */

unsigned int read_osns_record(char *buffer, unsigned int size, struct osns_record_s *r);
unsigned int write_osns_record(char *buffer, unsigned int size, const unsigned char type, void *ptr);
int compare_osns_record(struct osns_record_s *r, const unsigned char type, void *ptr);

int process_osns_records(char *data, unsigned int size, int (* cb)(struct osns_record_s *r, unsigned int count, unsigned int index, void *ptr), void *ptr);

#endif