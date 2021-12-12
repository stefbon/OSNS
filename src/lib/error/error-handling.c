/*

  2017 Stef Bon <stefbon@nomail.com>

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

#include "error-handling.h"

char *application_errors[] = {
	    "Unknown.",
	    "Protocol error.",
	    "Object not found.",
};

#ifdef __linux__

static char *get_description_system(struct generic_error_s *error)
{
    return strerror(error->value.errnum);
}

#else

static char *get_description_system(struct generic_error_s *error)
{
    return "Unknown system: unable to give error description";
}

#endif

static char *get_description_init(struct generic_error_s *error)
{
    return "";
}

static char *get_description_unknown_app(struct generic_error_s *error)
{
    return "Unknown application: unable to give error description";
}

static char *get_description_application(struct generic_error_s *error)
{
    return (error->value.errnum>=0 && error->value.errnum<=2) ? application_errors[error->value.errnum] : application_errors[0];
}

#ifdef __linux__

void set_generic_error_system(struct generic_error_s *error, int errnum, const char *function)
{
    if (error==NULL) return;
    error->type=_ERROR_TYPE_SYSTEM;
    error->value.errnum=errnum;
    error->get_description=get_description_system;
    if (function) strcpy(error->function, function);
}

#endif

void set_generic_error_application(struct generic_error_s *error, int errnum, char *(* get_desc)(struct generic_error_s *e), const char *function)
{
    if (error==NULL) return;

    error->type=_ERROR_TYPE_APPLICATION;
    error->value.errnum=errnum;

    if (get_desc) {

	error->get_description=get_desc;

    } else {

	error->get_description=get_description_application; /* for now ... howto register an application error handler here */

    }

    if (function) strcpy(error->function, function);
}

void init_generic_error(struct generic_error_s *error)
{
    memset(error, 0, sizeof(struct generic_error_s));
    error->get_description=get_description_init;
}

char *get_error_description(struct generic_error_s *error)
{
    if (error) return (* error->get_description)(error);
    return "";
}

struct generic_error_s initge = {
		    .type=0,
		    .value.errnum=0,
		    .get_description=get_description_init};

