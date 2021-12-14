/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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
#include <sys/stat.h>

#include "log.h"
#include "main.h"
#include "misc.h"
#include "datatypes.h"

#include "ssh-common.h"
#include "ssh-utils.h"

void init_ssh_pubkey(struct ssh_session_s *session)
{
    struct ssh_pubkey_s *pubkey=&session->pubkey;

    pubkey->pkalgo_client=0;
    pubkey->pksign_client=0;
    pubkey->pkcert_client=0;

    pubkey->pkalgo_server=0;
    pubkey->pksign_server=0;
    pubkey->pkcert_server=0;

}

void enable_algo_pubkey(struct ssh_session_s *session, int index, const char *who, const char *what)
{
    struct ssh_pubkey_s *pubkey=&session->pubkey;

    if (index>= (8 * sizeof(pubkey->pkalgo_client))) {

	logoutput_error("enable_bit_algo_pubkey: invalid value, too big: %i", index);
	return;

    }

    if (strcmp(who, "client")==0) {

	if (strcmp(what, "pk")==0) {

	    pubkey->pkalgo_client |= (1 << index);

	} else if (strcmp(what, "pkcert")==0) {

	    pubkey->pkcert_client |= (1 << index);

	} else if (strcmp(what, "pksign")==0) {

	    pubkey->pksign_client |= (1 << index);

	}

    } else if (strcmp(who, "server")==0) {

	if (strcmp(what, "pk")==0) {

	    pubkey->pkalgo_server |= (1 << index);

	} else if (strcmp(what, "pkcert")==0) {

	    pubkey->pkcert_server |= (1 << index);

	} else if (strcmp(what, "pksign")==0) {

	    pubkey->pksign_server |= (1 << index);

	}

    }

}

/* build a bitwise number of pkalgo's used in this session/per keyexchange */

static void build_pubkey_list(struct ssh_string_s *list, struct ssh_session_s *session, const char *who, const char *what)
{
    struct ssh_pubkey_s *pubkey=&session->pubkey;
    char *sep=NULL;
    char tmp[list->len + 2];
    char *name=tmp;

    logoutput("build_pubkey_list: %s pklist %.*s (type=%s)", who, list->len, list->ptr, what);

    /* copy to temporary list to ease the wlinkg through this list */

    memcpy(tmp, list->ptr, list->len);
    tmp[list->len]=',';
    tmp[list->len+1]='\0';

    search:
    sep=memchr(name, ',', (unsigned int)(tmp + list->len + 1 - name));

    while (sep) {
	unsigned int len=(unsigned int)(sep - name);
	int index=0;
	struct ssh_pkalgo_s *pkalgo=NULL;

	*sep='\0';
	pkalgo=get_pkalgo(name, len, &index);

	if (pkalgo && index>=0) enable_algo_pubkey(session, index, who, what);

	*sep=',';
	name=sep+1;
	sep=NULL;
	if ((unsigned int) (name - tmp) < list->len + 1) sep=memchr(name, ',', (unsigned int)(tmp + list->len + 1 - name));

    }

}

void store_algo_pubkey_negotiation(struct ssh_session_s *session, struct ssh_string_s *clist_c, struct ssh_string_s *clist_s, const char *what)
{
    if (clist_c) build_pubkey_list(clist_c, session, "client", what);
    if (clist_s) build_pubkey_list(clist_s, session, "server", what);
}

void free_ssh_pubkey(struct ssh_session_s *session)
{
    init_ssh_pubkey(session);
}
