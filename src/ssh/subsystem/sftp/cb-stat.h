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

#ifndef SSH_SUBSYSTEM_SFTP_CB_STAT_H
#define SSH_SUBSYSTEM_SFTP_CB_STAT_H

/* prototypes */

int reply_sftp_attr_from_stat(struct sftp_subsystem_s *sftp, uint32_t id, struct sftp_valid_s *valid, struct system_stat_s *stat);

void sftp_op_stat(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data);
void sftp_op_lstat(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data);
void sftp_op_fstat(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data);

#endif
