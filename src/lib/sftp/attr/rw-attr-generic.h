/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020 Stef Bon <stefbon@gmail.com>

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

#ifndef LIB_SFTP_ATTR_RW_ATTR_GENERIC_H
#define LIB_SFTP_ATTR_RW_ATTR_GENERIC_H

struct hashed_attrcb_s {
    struct list_element_s		list;
    uint32_t				valid;
    unsigned char			version;
    unsigned int			count;
    unsigned int			len;
    unsigned int			stat_mask;
    unsigned char			cb[];
};

/* prototypes */

struct hashed_attrcb_s *lookup_hashed_attrcb(uint32_t valid, unsigned char version);
void create_hashed_attrcb(unsigned char version, unsigned int valid, unsigned char *cb, unsigned char count, unsigned int len, unsigned int stat_mask);

void parse_attributes_generic(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat, unsigned int valid);
void write_attributes_generic(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat, unsigned int valid);
void read_attributes_generic(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat, unsigned int valid);

unsigned int get_size_buffer_write_attributes(struct attr_context_s *actx, struct rw_attr_result_s *r, unsigned int valid);

unsigned int translate_valid_2_stat_mask(struct attr_context_s *actx, unsigned int valid, unsigned char what);
unsigned int translate_stat_mask_2_valid(struct attr_context_s *actx, unsigned int mask, unsigned char what);

void init_hashattr_generic();
void clear_hashattr_generic(unsigned char force);

#endif
