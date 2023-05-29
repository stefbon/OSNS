/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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

#include "libosns-basic-system-headers.h"

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-datatypes.h"

#include "fspath.h"

unsigned char fs_path_compare(struct fs_path_s *path, const unsigned char type, void *ptr, struct ssh_string_s *sub)
{
    char *pstart1=(char *)(path->buffer + path->start);
    unsigned int plen1=path->len;
    unsigned char result=0;
    char *pstart2=NULL;
    unsigned int plen2=0;

    switch (type) {

	case 's' : {
	    struct ssh_string_s *prefix=(struct ssh_string_s *) ptr;

	    plen2=prefix->len;
	    pstart2=prefix->ptr;
	    break;

	}

	case 'p' : {
	    struct fs_path_s *prefix=(struct fs_path_s *) ptr;

            if (prefix->buffer) {

	        plen2=prefix->len;
	        pstart2=&prefix->buffer[prefix->start];

            }

	    break;

	}

	case 'c' : {

	    pstart2=(char *) ptr;
	    plen2=strlen(pstart2);
	    break;

	}

    }

    if (pstart1 && pstart2) {

        logoutput_debug("fs_path_compare: compare %.*s and %.*s", plen1, pstart1, plen2, pstart2);

	if (plen1 == plen2) {

            if (memcmp(pstart1, pstart2, plen1)==0) result=1;

        } else if (plen1 > plen2) {

            if (memcmp(pstart1, pstart2, plen2)==0) {
                char *sstart=NULL;

                if (pstart2[plen2 - 1] == '/') {

                    result=2;
                    sstart=(char *)(pstart1 + plen2 - 1);

                } else if (pstart1[plen2] == '/') {

	            result=2;
		    sstart=(char *)(pstart1 + plen2);

	        }

                if (sub) {

    		    sub->ptr=sstart;
		    sub->len=(unsigned int)(pstart1 + plen1 - sstart);

                }

            }

	}

    }

    return result;

}
