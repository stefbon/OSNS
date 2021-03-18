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

#ifndef OSNS_SSH_SUBSYSTEM_SFTP_SUPPORTED_H
#define OSNS_SSH_SUBSYSTEM_SFTP_SUPPORTED_H

/* prototypes */

unsigned int get_attrib_extensions(struct sftp_subsystem_s *sftp, char *buffer, unsigned int size, unsigned int *count);
unsigned int get_sftp_extensions(struct sftp_subsystem_s *sftp, char *buffer, unsigned int size, unsigned int *count);
unsigned int get_supported_attribute_flags(struct sftp_subsystem_s *sftp);
unsigned int get_supported_attribute_bits(struct sftp_subsystem_s *sftp);
unsigned int get_supported_open_flags(struct sftp_subsystem_s *sftp);
unsigned int get_supported_acl_access_mask(struct sftp_subsystem_s *sftp);
unsigned int get_supported_max_readsize(struct sftp_subsystem_s *sftp);
unsigned int get_supported_open_block_flags(struct sftp_subsystem_s *sftp);
unsigned int get_supported_block_flags(struct sftp_subsystem_s *sftp);

unsigned int get_supported_acl_cap(struct sftp_subsystem_s *sftp);

#endif
