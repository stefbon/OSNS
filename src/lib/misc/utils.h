/*

  2010, 2011 Stef Bon <stefbon@gmail.com>

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
#ifndef _LIB_MISC_UTILS_H
#define _LIB_MISC_UTILS_H

#define UTILS_CONVERT_SKIPSPACE 1
#define UTILS_CONVERT_TOLOWER	2

#include <sys/stat.h>
#include <time.h>

struct common_buffer_s {
    unsigned int 				len;
    char					*pos;
    char					*ptr;
    unsigned int				size;
};

#define INIT_COMMON_BUFFER		{0, NULL, NULL, 0}

// Prototypes

void init_common_buffer(struct common_buffer_s *c_buffer);
void free_common_buffer(struct common_buffer_s *c_buffer);

void unslash(char *p);
void convert_to(char *string, int flags);

unsigned char belongtosameprocess(pid_t process_id, pid_t thread_id);
pid_t getprocess_id(pid_t thread_id);

int custom_fork();

uint32_t safe_atoi(char *b);
uint64_t safe_atoii(char *b);

void strdup_target_path(char *target, char **p_path, unsigned int *error);
int compare_starting_substring(char *name, unsigned int len, const char *start, unsigned int *p_pos);
int compare_name_what(const char *what, unsigned int len, const char *name, unsigned int *p_pos);

int compare_unsigned(unsigned int a, unsigned int b);

int dup_fd_another_process(pid_t pid, unsigned int fd);

#endif
