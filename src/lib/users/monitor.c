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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>

#include <systemd/sd-login.h>

#include "log.h"
#include "datatypes/ssh-string.h"

#include "monitor.h"

#define _MONITOR_FLAG_CHANGED				1
#define _MONITOR_FLAG_THREAD				2

struct _login_uids_s {
    uid_t		*uid;
    unsigned int	len;
};

static sd_login_monitor *monitor=NULL;
static struct _login_uids_s current = {.uid=NULL, .len=0};
static void *monitor_ptr=NULL;
static void (* monitor_cb)(uid_t uid, int what, void *ptr);
static int (* monitor_filter)(uid_t uid, void *ptr);
static unsigned char monitor_flags=0;
static pthread_mutex_t monitor_mutex=PTHREAD_MUTEX_INITIALIZER;

static void sort_array_uids(struct _login_uids_s *new)
{

    if (new->len==0 || new->len==1) return;

    logoutput("sort_array_uids: new len %i", new->len);

    for (unsigned int i=1; i<new->len; i++) {
	int j=i-1;

	while (j>=0) {

	    logoutput("sort_array_uids: i %i j %i", i, j);

	    /* compare element j and j+1 
               if element j has a bigger value, than swap and continue 
               if not than stop */

	    if (new->uid[j] > new->uid[j+1]) {
		uid_t tuid=new->uid[j];

		new->uid[j]=new->uid[j+1];
		new->uid[j+1]=tuid;

	    } else {

		break;

	    }

	    j--;

	}

    }

}

static void compare_uids(struct _login_uids_s *new)
{

    logoutput("compare_uids: current len %i new len %i", current.len, new->len);

    /* first check something really has changed */

    if (current.len==new->len) {

	if (current.len==0) {

	    return;

	} else if (memcmp(current.uid, new->uid, current.len)==0) {

	    free(new->uid);
	    new->uid=NULL;
	    new->len=0;
	    return;

	}

    }

    if (current.len==0) {

	for (unsigned int i=0; i<new->len; i++) {

	    if ((*monitor_filter)(new->uid[i], monitor_ptr)==0) (* monitor_cb)(new->uid[i], 1, monitor_ptr);

	}

	current.uid=new->uid;
	current.len=new->len;
	new->uid=NULL;
	new->len=0;

    } else if (new->len==0) {

	for (unsigned int i=0; i<current.len; i++) {

	    if ((*monitor_filter)(current.uid[i], monitor_ptr)==0) (* monitor_cb)(current.uid[i], -1, monitor_ptr);

	}

	free(current.uid);
	current.uid=NULL;
	current.len=0;

    } else {
	unsigned i=0,j=0;

	/* walk through both lists, i is index in current, j in new */

	while(1) {

	    if (i<current.len && j<new->len) {

		if (new->uid[j]==current.uid[i]) {

		    i++;
		    j++;
		    continue;

		} else if (new->uid[j]<current.uid[i]) {

		    if ((*monitor_filter)(new->uid[i], monitor_ptr)==0) (* monitor_cb)(new->uid[j], 1, monitor_ptr);
		    j++;
		    continue;

		} else {

		    if ((*monitor_filter)(current.uid[i], monitor_ptr)==0) (* monitor_cb)(current.uid[i], -1, monitor_ptr);
		    i++;
		    continue;

		}

	    } else if (i<current.len) {

		if ((*monitor_filter)(current.uid[i], monitor_ptr)==0) (* monitor_cb)(current.uid[i], -1, monitor_ptr);
		i++;
		continue;

	    } else if (j<new->len) {

		if ((*monitor_filter)(new->uid[i], monitor_ptr)==0) (* monitor_cb)(new->uid[j], 1, monitor_ptr);
		j++;
		continue;

	    } else {

		break;

	    }

	}

	free(current.uid);
	current.uid=new->uid;
	current.len=new->len;

	new->uid=NULL;
	new->len=0;

    }

}

static void user_monitor_cb_dummy(uid_t uid, int what, void *ptr)
{
    logoutput("user_monitor_cb_dummy: %s %i", (what==1) ? "added" : "removed", uid);
}

static int user_monitor_filter_dummy(uid_t uid, void *ptr)
{
    return 0;	/* by default do not filter any uid */
}

int create_user_monitor(void (* cb)(uid_t uid, int what, void *ptr), void *ptr, int (* filter)(uid_t uid, void *ptr))
{
    int fd=-1;

    monitor_cb=(cb) ? cb : user_monitor_cb_dummy;
    monitor_filter=(filter) ? filter : user_monitor_filter_dummy;
    monitor_ptr=ptr;

    if (sd_login_monitor_new("uid", &monitor)==0) {

	logoutput("create_user_monitor: monitor created");
	fd=sd_login_monitor_get_fd(monitor);

    } else {

	logoutput("create_user_monitor: failed to create monitor");

    }

    return fd;

}

void close_user_monitor()
{
    sd_login_monitor_unrefp(&monitor);
    monitor=NULL;
}

void read_user_monitor_event(int fd, void *data, struct event_s *event)
{
    struct _login_uids_s new = {.uid=NULL, .len=0};
    int result=0;
    uid_t *uid=NULL;

    pthread_mutex_lock(&monitor_mutex);

    if (monitor_flags & _MONITOR_FLAG_THREAD) {

	monitor_flags |= _MONITOR_FLAG_CHANGED;
	pthread_mutex_unlock(&monitor_mutex);
	return 0;

    }

    process:

    logoutput("read_user_monitor_event");

    monitor_flags |= _MONITOR_FLAG_THREAD;
    monitor_flags &= ~_MONITOR_FLAG_CHANGED;
    pthread_mutex_unlock(&monitor_mutex);

    sd_login_monitor_flush(monitor);

    result=sd_get_uids(&uid);

    if (result>=0) {

	new.len=result;
	new.uid=uid;
        sort_array_uids(&new);
	compare_uids(&new);

    } else {

	result=abs(result);
	logoutput("read_user_monitor_event: sd_get_uids returned %i:%s", result, strerror(result));

    }

    pthread_mutex_lock(&monitor_mutex);
    if (monitor_flags & _MONITOR_FLAG_CHANGED) goto process;
    monitor_flags &= ~_MONITOR_FLAG_THREAD;
    pthread_mutex_unlock(&monitor_mutex);

    logoutput("read_user_monitor_event: finish");

    return 0;

}
