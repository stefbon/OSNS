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

#ifndef SSH_SUBSYSTEM_SFTP_ATTRIBUTES_WRITE_H
#define SSH_SUBSYSTEM_SFTP_ATTRIBUTES_WRITE_H

/* prototypes */

unsigned int write_attributes_len(struct sftp_subsystem_s *s, struct sftp_attr_s *attr, struct stat *st, unsigned int valid);
unsigned int write_attributes(struct sftp_subsystem_s *s, char *buffer, unsigned int size, struct sftp_attr_s *attr, unsigned int valid);

unsigned int write_readdir_attr(struct sftp_subsystem_s *s, char *buffer, unsigned int size, char *name, unsigned int len, struct stat *st, unsigned int valid, unsigned int *error);

#endif
