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
#ifndef OSNS_SSH_SUBSYSTEM_COMMONHANDLE_H
#define OSNS_SSH_SUBSYSTEM_COMMONHANDLE_H

#define COMMONHANDLE_TYPE_FILE				1
#define COMMONHANDLE_TYPE_DIR				2

struct commonhandle_s {
    unsigned char					type;
    dev_t						dev;
    uint64_t						ino;
    unsigned int					pid;
    unsigned int					fd;
    void						(* close)(struct commonhandle_s *handle);
    void						(* free)(struct commonhandle_s **handle);
};

// Prototypes

int writelock_commonhandles(struct simple_lock_s *lock);
int readlock_commonhandles(struct simple_lock_s *lock);
int unlock_commonhandles(struct simple_lock_s *lock);

void insert_commonhandle_hash(struct commonhandle_s *handle);
void remove_commonhandle_hash(struct commonhandle_s *handle);
unsigned int calculate_ino_hash(uint64_t ino);

struct commonhandle_s *get_next_commonhandle(void **index, unsigned int hashvalue);

int init_commonhandles(unsigned int *error);
void free_commonhandles();

unsigned char write_commonhandle(struct commonhandle_s *handle, char *buffer);

#endif
