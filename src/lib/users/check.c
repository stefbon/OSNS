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

#include "datatypes/ssh-string.h"
#include "libosns-log.h"

/* check for existance of username
    if not, return -1
    if yes,
	and passwd is empty, return 0
	and passwd is not empty, check the passwd also, if correct, return 0, if not return -2

    special case: when username longer than 255, return also -1
*/


#ifdef HAVE_LIBPAM

#include <security/pam_appl.h>

struct _tmp_pamconv_s {
    const char			*username;
    const char			*password;
};

static int pam_password_conversation(int num, const struct pam_message **p_msg, struct pam_response **p_resp, void *ptr)
{
    const struct pam_message *msg=*p_msg;
    struct pam_response *resp=NULL;
    struct _tmp_pamconv_s *tmp=(struct _tmp_pamconv_s *) ptr;

    resp=calloc(sizeof(struct pam_response), num);
    if (resp==NULL) return PAM_BUF_ERR;
    *p_resp=resp;

    for (unsigned int i=0; i<num; i++) {

	switch (msg[i].msg_style) {

	    case PAM_PROMPT_ECHO_OFF:
	    case PAM_PROMPT_ECHO_ON:
		resp[i].resp=strdup(tmp->password);
		break;
	    case PAM_ERROR_MSG:
	    case PAM_TEXT_INFO:
		logoutput("pam_password_conversation: %s", msg[i].msg);
		break;

	}

    }
    return PAM_SUCCESS;

}

int check_username_password_pam(const char *user, const char *pw)
{
    struct _tmp_pamconv_s tmp={.username=user, .password=pw};
    const struct pam_conv pam_conversation = {pam_password_conversation, (void *) &tmp};
    pam_handle_t *handle = NULL;
    int retcode=pam_start("system-remote-login", user, &pam_conversation, &handle);
    int result=-1;

    if (retcode!=PAM_SUCCESS) {

        logoutput("check_username_password_pam: pam_start returned: %i", retcode);
        return -1;

    }

    if (pw) {

	retcode = pam_authenticate(handle, 0);

	if (retcode == PAM_SUCCESS) {

	    result=0; /* success */

	} else {

	    logoutput("check_username_password_pam: authentication error (%i:%s)", retcode, pam_strerror(handle, retcode));

	}

    }

    retcode=pam_acct_mgmt(handle, 0);

    if (retcode == PAM_SUCCESS) {

	if (pw==NULL) result=0;

    } else {

	logoutput("check_username_password_pam: acct mgmt error (%i:%s)", retcode, pam_strerror(handle, retcode));

    }

    retcode = pam_end(handle, retcode);
    return result;

}

int check_username_password(struct ssh_string_s *username, struct ssh_string_s *password)
{

    if (username->len>0 && username->len<256) {
	char user[username->len + 1];

	memcpy(user, username->ptr, username->len);
	user[username->len]='\0';

	if (password==NULL) {

	    return check_username_password_pam(user, NULL);

	} else if (password->len>0 && password->len<256) {
	    char pw[password->len + 1];

	    memcpy(pw, password->ptr, password->len);
	    pw[password->len]='\0';

	    return check_username_password_pam(user, pw);

	}

    }

    return -1;

}

#else

#include <pwd.h>
#include <grp.h>
#include <shadow.h>
#include <crypt.h>

int check_username_password(struct ssh_string_s *username, struct ssh_string_s *password)
{
    if (username->len<256) {
	char name[username->len + 1];
	struct passwd *pwd=NULL;

	lock_local_userbase();

	memcpy(name, username->ptr, username->len);
	name[username->len]='\0';
	pwd=getpwnam(name);

	if (password==NULL) {
	    int result=(pwd) ? 0 : -1;

	    unlock_local_userbase();
	    return result;

	} else {
	    char pass[password->len + 1];

	    memcpy(pass, password->ptr, password->len)
	    pass[password->len]='\0';

	    if (strcmp(pwd->pw_passwd, "x")==0) {
		struct spwd *spwd=getspnam(name);
		int result=(spwd==NULL) ? -2 : (strcmp(spwd->sp_pwdp, crypt(pass, spwd->sp_pwdp)) ? -2 : 0);

		unlock_local_userbase();
		return result;

	    } else {
		int result=(strcmp(pwd->pw_passwd, crypt(pass, pwd->pw_passwd)) ? -2 : 0);

		unlock_local_userbase();
		return result;

	    }

	}

	unlock_local_userbase();

    }

    return -1;
}

#endif