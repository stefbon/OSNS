/*
  2010, 2011, 2012 Stef Bon <stefbon@gmail.com>

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

#include <sys/syscall.h>
#include <sys/wait.h>
#include <pthread.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-list.h"
#include "libosns-error.h"
#include "libosns-system.h"

#include "workerthreads.h"

static pthread_mutex_t thread_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t thread_cond=PTHREAD_COND_INITIALIZER;

static struct shared_signal_s thread_shared_signal = {

	.flags				= 0,
	.mutex				= &thread_mutex,
	.cond				= &thread_cond,
	.lock				= _signal_default_lock,
	.unlock				= _signal_default_unlock,
	.broadcast			= _signal_default_broadcast,
	.condwait			= _signal_default_condwait,
	.condtimedwait			= _signal_default_condtimedwait

};

struct threadjob_s {
    void 						(* cb)(void *ptr);
    void 						*ptr;
    struct list_element_s 				list;
};

struct wt_s {
    pthread_t 						threadid;
    struct list_element_s				list;
    struct list_element_s				wlist;
};

#define WT_QUEUE_STATUS_FINISH                          1

struct wt_queue_s {
    unsigned int                                        status;
    struct list_header_s 				threads;
    struct list_header_s				joblist;
    unsigned int 					max_nrthreads;
};

static struct wt_queue_s default_queue;

static struct wt_queue_s *get_wt_queue(struct wt_s *thread)
{
    struct list_header_s *h=NULL;

    read_lock_list_element(&thread->list);
    h=thread->list.h;
    read_unlock_list_element(&thread->list);

    return (h) ? ((struct wt_queue_s *)((char *)h - offsetof(struct wt_queue_s, threads))) : NULL;
}

static struct threadjob_s *get_next_job(struct wt_queue_s *queue)
{
    struct list_header_s *jobs=&queue->joblist;
    struct list_element_s *list=NULL;

    write_lock_list_header(jobs);
    list=remove_list_head(jobs);
    write_unlock_list_header(jobs);

    return (list) ? ((struct threadjob_s *) ((char *) list - offsetof(struct threadjob_s, list))) : NULL;
}

static void process_thread_jobs(void *ptr)
{
    struct wt_s *thread=NULL;
    struct wt_queue_s *queue=NULL;
    struct list_element_s *list=NULL;
    sigset_t emptyset;

    if (ptr==NULL) return;
    thread=(struct wt_s *) ptr;
    queue=get_wt_queue(thread);
    if (queue==NULL) goto exitthread;
    thread->threadid=pthread_self();

    sigemptyset(&emptyset);
    pthread_sigmask(SIG_BLOCK, &emptyset, NULL);

    /* thread can be cancelled any time */

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    checkandwait:

    signal_lock(&thread_shared_signal);

    if (queue->status & WT_QUEUE_STATUS_FINISH) {

        signal_unlock(&thread_shared_signal);
        goto exitthread;

    }

    /* wait till this thread has to do some work */

    write_lock_list_header(&queue->joblist);
    list=remove_list_head(&queue->joblist);
    write_unlock_list_header(&queue->joblist);

    if (list) {
        struct threadjob_s *job=(struct threadjob_s *) ((char *) list - offsetof(struct threadjob_s, list));

        signal_unlock(&thread_shared_signal);
	(* job->cb)(job->ptr);
	free(job);
        goto checkandwait;

    }

    int tmp=signal_condwait(&thread_shared_signal);
    signal_unlock(&thread_shared_signal);
    goto checkandwait;

    exitthread:

    write_lock_list_header(&queue->threads);
    remove_list_element(&thread->list);
    write_unlock_list_header(&queue->threads);

    free(thread);
    pthread_exit(NULL);

}

static struct wt_s *create_workerthread(struct wt_queue_s *queue)
{
    struct wt_s *thread=NULL;
    int result=-1;
    pthread_attr_t attr;

    thread=malloc(sizeof(struct wt_s));
    if (thread==NULL) return NULL;

    memset(thread, 0, sizeof(struct wt_s));
    thread->threadid=0;
    init_list_element(&thread->list, NULL);

    result=pthread_attr_init(&attr);

    if (result) {

	free(thread);
	return NULL;

    }

    result=pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (result) {

	free(thread);
	pthread_attr_destroy(&attr);
	return NULL;

    }

    result=pthread_create(&thread->threadid, &attr, (void *) process_thread_jobs, (void *) thread);
    pthread_attr_destroy(&attr);

    if (result) {

	logoutput_warning("create_workerthread: error %i:%s starting thread", result, strerror(result));
	free(thread);
	thread=NULL;

    }

    return thread;

}

static int add_workerthread(struct wt_queue_s *queue)
{
    struct wt_s *thread=create_workerthread(queue);

    if (thread) {

        write_lock_list_header(&queue->threads);
        add_list_element_last(&queue->threads, &thread->list);
        write_unlock_list_header(&queue->threads);

    }

    return (thread) ? 0 : -1;

}

void work_workerthread(void *ptr, int timeout, void (*cb)(void *ptr), void *data)
{
    struct wt_queue_s *queue=(ptr) ? (struct wt_queue_s *) ptr : &default_queue;
    struct threadjob_s *job=NULL;

    if (queue->status & WT_QUEUE_STATUS_FINISH) return;

    job=malloc(sizeof(struct threadjob_s));
    if (! job) return;
    job->cb=cb;
    job->ptr=data;
    init_list_element(&job->list, NULL);

    write_lock_list_header(&queue->joblist);
    add_list_element_last(&queue->joblist, &job->list);
    write_unlock_list_header(&queue->joblist);

    signal_broadcast_locked(&thread_shared_signal);

}

void init_workerthreads(void *ptr)
{
    struct wt_queue_s *queue=(ptr) ? (struct wt_queue_s *) ptr : &default_queue;

    init_list_header(&queue->threads, SIMPLE_LIST_TYPE_EMPTY, NULL);
    init_list_header(&queue->joblist, SIMPLE_LIST_TYPE_EMPTY, NULL);
    queue->max_nrthreads=6;
    queue->status=0;

}

void stop_workerthreads(void *ptr)
{
    struct wt_queue_s *queue=(ptr) ? (struct wt_queue_s *) ptr : &default_queue;

    signal_lock(&thread_shared_signal);
    queue->status |= WT_QUEUE_STATUS_FINISH;
    signal_broadcast(&thread_shared_signal);
    signal_unlock(&thread_shared_signal);

}

void terminate_workerthreads(void *ptr, unsigned int timeout)
{
    struct wt_queue_s *queue=(ptr) ? (struct wt_queue_s *) ptr : &default_queue;
    stop_workerthreads(ptr);
}

void set_max_numberthreads(void *ptr, unsigned maxnr)
{
    struct wt_queue_s *queue=(ptr) ? (struct wt_queue_s *) ptr : &default_queue;

    queue->max_nrthreads=maxnr;
}

void start_default_workerthreads(void *ptr)
{
    struct wt_queue_s *queue=(ptr) ? (struct wt_queue_s *) ptr : &default_queue;
    for (unsigned int i=0; i<queue->max_nrthreads; i++) add_workerthread(queue);
}
