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

#ifndef INTERFACE_SMB_SIGNAL_H
#define INTERFACE_SMB_SIGNAL_H

#define SMB_SIGNAL_FLAG_CONNECTING					1
#define SMB_SIGNAL_FLAG_CONNECTED					2
#define SMB_SIGNAL_FLAG_DISCONNECTING					4
#define SMB_SIGNAL_FLAG_DISCONNECTED					8
#define SMB_SIGNAL_FLAG_ERROR						16

#define SMB_SIGNAL_FLAG_CONNECT						( SMB_SIGNAL_FLAG_CONNECTING | SMB_SIGNAL_FLAG_CONNECTED )
#define SMB_SIGNAL_FLAG_DISCONNECT					( SMB_SIGNAL_FLAG_DISCONNECTING | SMB_SIGNAL_FLAG_DISCONNECTED )

struct smb_signal_s {
    unsigned int							flags;
    struct common_signal_s						*signal;
    unsigned int							error;
};

/* prototypes */

int smb_signal_lock(struct smb_signal_s *s);
int smb_signal_unlock(struct smb_signal_s *s);
int smb_signal_condtimedwait(struct smb_signal_s *s, struct timespec *e);
int smb_signal_broadcast(struct smb_signal_s *s);

#endif
