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

#ifndef INTERFACE_SMB_EVENTLOOP_H
#define INTERFACE_SMB_EVENTLOOP_H

/* prototypes */

short translate_bevent_to_poll(struct event_s *event);
void translate_poll_to_bevent(struct bevent_s *bevent, short events);
void process_smb_share_event(int fd, void *ptr, struct event_s *event);
int wait_smb_share_connected(struct context_interface_s *interface, struct timespec *timeout);

void _smb2_change_fd_cb(struct smb2_context *smb2, int fd, int cmd);
void _smb2_change_events_cb(struct smb2_context *smb2, int fd, int events);

#endif
