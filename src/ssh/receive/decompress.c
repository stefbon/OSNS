/*
  2017, 2018 Stef Bon <stefbon@gmail.com>

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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>

#include "log.h"
#include "main.h"
#include "misc.h"

#include "ssh-common.h"
#include "ssh-utils.h"
#include "ssh-receive.h"

static struct list_header_s list_decompress_ops=INIT_LIST_HEADER;

struct decompress_ops_s *get_decompress_ops_container(struct list_element_s *list)
{
    return (struct decompress_ops_s *) (((char *) list) - offsetof(struct decompress_ops_s, list));
}

void add_decompress_ops(struct decompress_ops_s *ops)
{
    add_list_element_last(&list_decompress_ops, &ops->list);
}

struct decompress_ops_s *get_next_decompress_ops(struct decompress_ops_s *ops)
{
    struct list_element_s *list=NULL;

    if (ops) {

	list=get_next_element(&ops->list);

    } else {

	list=get_list_head(&list_decompress_ops, 0);

    }

    return (list) ? get_decompress_ops_container(list) : NULL;;
}


void reset_decompress(struct ssh_connection_s *connection, struct algo_list_s *algo_compr)
{
    struct ssh_receive_s *receive=&connection->receive;
    struct ssh_decompress_s *decompress=&receive->decompress;
    struct decompress_ops_s *ops=(struct decompress_ops_s *) algo_compr->ptr;

    remove_decompressors(decompress);
    memset(decompress->name, '\0', sizeof(decompress->name));

    decompress->ops=ops;
    strcpy(decompress->name, algo_compr->sshname);

}

unsigned int build_compress_list_s2c(struct ssh_connection_s *connection, struct algo_list_s *alist, unsigned int start)
{
    struct decompress_ops_s *ops=NULL;

    ops=get_next_decompress_ops(NULL);

    while (ops) {

	start=(* ops->populate)(connection, ops, alist, start);
	ops=get_next_decompress_ops(ops);

    }

    return start;

}

void init_decompress_once()
{
    init_list_header(&list_decompress_ops, SIMPLE_LIST_TYPE_EMPTY, 0);
}
