/*
  2016, 2017, 2018 Stef Bon <stefbon@gmail.com>

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
#include <shadow.h>
#include <crypt.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-network.h"

#include "ssh-common.h"
#include "ssh-common-protocol.h"
#include "ssh-connections.h"
#include "ssh-receive.h"
#include "ssh-send.h"
#include "ssh-hostinfo.h"
#include "ssh-utils.h"
#include "utils.h"

/*
    generic function to read the comma seperated name list of names of authentications that can continue
    used when processing the MSG_USERAUTH_FAILURE response
*/

static unsigned int get_required_auth_methods(char *namelist, unsigned int len)
{
    unsigned int methods=0;
    char list[len+1];
    char *pos=&list[0];
    char *sep=NULL;

    memcpy(list, namelist, len);
    list[len]='\0';

    logoutput("get_required_auth_methods: %s", list);

    findmethod:

    sep=strchr(pos, ',');
    if (sep) *sep='\0';

    if (strcmp(pos, "publickey")==0) {

	methods|=SSH_AUTH_METHOD_PUBLICKEY;

    } else if (strcmp(pos, "password")==0) {

	methods|=SSH_AUTH_METHOD_PASSWORD;

    } else if (strcmp(pos, "hostbased")==0) {

	methods|=SSH_AUTH_METHOD_HOSTBASED;

    } else {

	methods|=SSH_AUTH_METHOD_UNKNOWN;

    }

    if (sep) {

	*sep=',';
	pos=sep+1;
	goto findmethod;

    }

    return methods;

}

unsigned int write_required_auth_methods(char *buffer, unsigned int size, unsigned int methods)
{
    unsigned int pos=0;
    if (methods & SSH_AUTH_METHOD_PUBLICKEY) pos+= ((buffer) ? snprintf(&buffer[pos], size-pos, "%spublickey", (pos>0) ? "," : "") : strlen("publickey") + 2);
    if (methods & SSH_AUTH_METHOD_PASSWORD) pos+= ((buffer) ? snprintf(&buffer[pos], size-pos, "%spassword", (pos>0) ? "," : "") : strlen("password") + 2);
    if (methods & SSH_AUTH_METHOD_HOSTBASED) pos+= ((buffer) ? snprintf(&buffer[pos], size-pos, "%shostbased", (pos>0) ? "," : "") : strlen("hostbased") + 2);
    return pos;
}

/* generic function to handle the userauth failure response
    see: https://tools.ietf.org/html/rfc4252#section-5.1 Responses to Authentication Request

    message looks like:
    - byte			SSH_MSG_USERAUTH_FAILURE
    - name-list			authentications that can continue
    - boolean			partial success

    NOTE:
    if partial success is false then the auth method offered has failed
*/

int handle_auth_failure(struct ssh_payload_s *payload, struct ssh_auth_s *auth)
{
    unsigned int result=-1;

    if (payload->len>6) {
	unsigned int len=get_uint32(&payload->buffer[1]);

	if (len>0 && payload->len==6+len) {
	    unsigned char partial=(unsigned char) payload->buffer[5+len];

	    auth->required=get_required_auth_methods(&payload->buffer[5], len);
	    result=(partial>0) ? 0 : -1;

	}

    }

    logoutput("handle_auth_failure: result %i", result);
    return result;
}

void init_pwlist(struct pw_list_s *pwlist)
{
    pwlist->type=0;
    pwlist->pword.user=NULL;
    pwlist->pword.pw=NULL;
    pwlist->next=NULL;
}

void free_pwlist(struct pw_list_s *pwlist)
{
    struct pw_list_s *list=pwlist;

    dofree:

    if (list) {
	struct pw_list_s *next=list->next;

	if (list->pword.user) free(list->pword.user);
	if (list->pword.pw) free(list->pword.pw);

	free(list);
	list=next;
	goto dofree;

    }

}


#define READ_CREDENTIALS_USER					1
#define READ_CREDENTIALS_PW					2

int read_credentials(char *path, struct pword_s *pword)
{
    FILE *fp=fopen(path, "r");
    int result=0;

    if (fp) {
	size_t size=0;
	char *line=NULL;
	char *sep=NULL;

	logoutput("read_credentials: found %s", path);

	while (getline(&line, &size, fp)>0) {

	    sep=memchr(line, '\n', size);
	    if (sep) *sep='\0';
	    size=strlen(line);
	    if (size==0) continue;

	    sep=memchr(line, '=', size);

	    if (sep) {
		char option[sep - line + 1];
		char *value=sep + 1;

		memcpy(option, line, (unsigned int)(sep - line));
		option[(unsigned int)(sep - line + 1)]='\0';

		if (strcmp(option, "user")==0) {

		    pword->user=strdup(value);
		    if (pword->user) result|=READ_CREDENTIALS_USER;

		} else if (strcmp(option, "pw")==0) {

		    pword->pw=strdup(value);
		    if (pword->pw) result|=READ_CREDENTIALS_PW;

		}

	    }

	}

	if (line) free(line);
	fclose(fp);

    } else {

	logoutput("read_credentials: %s tried, not open", path);

    }

    logoutput("read_credentials: result %i", result);

    return result;

}

unsigned int read_private_pwlist(struct ssh_connection_s *connection, struct pw_list_s **pwlist)
{
    struct ssh_session_s *session=get_ssh_connection_session(connection);
    struct pw_list_s *list=*pwlist;
    unsigned int len = strlen(session->identity.pwd.pw_dir) + 2;
    char *hostname=NULL;
    char *ipv4=NULL;
    unsigned int count=0;
    unsigned int error=0;
    int fd=-1;

    if (list) {
	struct pword_s *pword=NULL;
	struct pw_list_s *element=list;

	while (element) {

	    list=element->next;
	    free_pwlist(element);
	    element=list;

	}

	*pwlist=NULL;
	list=NULL;

    }

    /* first: most generic:
	look for the pw in ~/.auth/cred */

    len = strlen(session->identity.pwd.pw_dir) + 2 + strlen(".auth/cred");

    if (len>0) {
	char path[len];

	if (snprintf(path, len, "%s/.auth/cred", session->identity.pwd.pw_dir)>0) {
	    struct pw_list_s *element=NULL;

	    element=malloc(sizeof(struct pw_list_s));
	    if (element==NULL) goto out;
	    init_pwlist(element);
	    element->type=PW_TYPE_GLOBAL;

	    if (read_credentials(path, &element->pword)==(READ_CREDENTIALS_USER|READ_CREDENTIALS_PW)) {

		element->next=*pwlist;
		*pwlist=element;
		count++;

	    } else {

		free_pwlist(element);

	    }

	}

    }

    out:

    logoutput("read_private_pwlist: count %i", count);
    return count;

}

struct pw_list_s *get_next_pwlist(struct pw_list_s *pwlist, struct pw_list_s *element)
{
    if (element==NULL) return pwlist;
    return element->next;
}

int select_userauth_reply(struct ssh_payload_s *payload, void *ptr)
{
    return (((payload->type == SSH_MSG_USERAUTH_SUCCESS) || (payload->type == SSH_MSG_USERAUTH_FAILURE)) ? 1 : 0);
}

int respond_userauth_request(struct ssh_connection_s *connection, unsigned char success)
{
    struct ssh_setup_s *setup=&connection->setup;
    struct ssh_auth_s *auth=&setup->phase.service.type.auth;

    return send_userauth_request_reply(connection, auth->required, success);
}

int test_algo_publickey(struct ssh_connection_s *connection, struct ssh_pkalgo_s *pkalgo)
{
    return test_pkalgo_ability(pkalgo->sshname, "sign");
}
