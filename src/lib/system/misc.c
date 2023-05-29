/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021 Stef Bon <stefbon@gmail.com>

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

#include <sys/stat.h>
#include <fcntl.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-datatypes.h"

int run_command_system(struct fs_location_path_s *command, char *param, void (* cb_readoutput)(char *line, unsigned int len, void *ptr), void (* cb_error)(unsigned int errcode, void *ptr), void *ptr)
{
    char dummy="";
    char *tmp=(param) ? param : &dummy;
    unsigned int len=command->len + strlen(tmp) + 16;
    char buffer[len];
    int result=0;

    if (snprintf(buffer, len, "%.*s %s", command->len, command->ptr, tmp)>0) {
        FILE *fp=popen(buffer, "r");

	if (fp) {
	    char *tmpline=NULL;
	    size_t len=0;
	    int bytesread=getline(&tmpline, &len, fp);

	    while (bytesread>0) {

		char *sep=strchr(tmpline, '\n');
		if (sep) *sep='\0';

                (* cb_readoutput)(tmpline, strlen(tmpline), ptr);
                bytesread=getline(&tmpline, &len, fp);

	    }

	    pclose(fp);
	    if (tmpline) free(tmpline);

	} else {

            (* cb_error)(errno, ptr);
	    result=-1;

	}

    }

    return result;

}
