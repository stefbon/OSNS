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

#include <pwd.h>
#include <grp.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-threads.h"
#include "libosns-interface.h"

#include "ssh-common.h"
#include "mapping.h"

/*
    get the uid and gid of the unknown user and group
    uid and gid of the remote host which are not reckognized
    (== not the connecting user/group, not root and not nobody...)
    get this uid/gid
*/

void get_local_unknown_user(struct ssh_session_s *session)
{
    struct ssh_hostinfo_s *hostinfo=&session->hostinfo;
    struct net_idmapping_s *mapping=&hostinfo->mapping;
    struct passwd *pwd=NULL;
    char *user=NULL;
    struct io_option_s option;

    mapping->unknown_uid=(uid_t) -1;
    mapping->unknown_gid=(gid_t) -1;

    pwd=getpwnam("unknown");
    if (pwd) goto found;

    /* try user "nobody" */

    pwd=getpwnam("nobody");
    if (pwd) goto found;
    logoutput("get_local_unknown_user: no user found ");
    return;

    found:

    logoutput("get_local_unknown_user: user %i:%s", pwd->pw_uid, pwd->pw_name);
    mapping->unknown_uid=pwd->pw_uid;
    mapping->unknown_gid=pwd->pw_gid;

}
