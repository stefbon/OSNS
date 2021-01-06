/*
 
  2017 Stef Bon <stefbon@gmail.com>

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "log.h"

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

#include <avahi-common/cdecl.h>
#include <avahi-common/thread-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/domain.h>

#include "misc.h"
#include "workspace-interface.h"
#include "eventloop.h"
#include "discover.h"

static AvahiThreadedPoll *threadedpoll = NULL;
static AvahiClient *client = NULL;
static AvahiStringList *types = NULL;

extern void add_net_service_avahi(const char *type, char *hostname, char *ipv4, unsigned int port);

static void service_resolver_cb(AvahiServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event,
				const char *name, const char *type, const char *domain, const char *hostname, const AvahiAddress *a, uint16_t port,
				AvahiStringList *txt, AVAHI_GCC_UNUSED AvahiLookupResultFlags flags, void *userdata)
{

    switch (event) {

        case AVAHI_RESOLVER_FOUND: {

	    if (a->proto==AVAHI_PROTO_INET) {
    		char ipv4[AVAHI_ADDRESS_STR_MAX];

		avahi_address_snprint(ipv4, AVAHI_ADDRESS_STR_MAX, a);
		logoutput("service_resolver_cb: found ipv4 %s", ipv4);
		add_net_service_generic(type, (char *)hostname, ipv4, port, DISCOVER_METHOD_AVAHI);

	    }

            break;
        }

        case AVAHI_RESOLVER_FAILURE:

            logoutput_warning("service_resolver_cb: failed to resolve service '%s' of type '%s' in domain '%s': %s", name, type, domain, avahi_strerror(avahi_client_errno(client)));
            break;
    }


    avahi_service_resolver_free(r);

}

static void service_browser_cb(AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name, const char *type, const char *domain, AvahiLookupResultFlags flags, void *ptr)
{

    switch (event) {

        case AVAHI_BROWSER_NEW: {

            if (flags & AVAHI_LOOKUP_RESULT_LOCAL) break;

	    if (! avahi_service_resolver_new(client, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, 0, service_resolver_cb, NULL)) {

		logoutput_info("service_browser_cb: failed to resolve %s", name, domain, avahi_strerror(avahi_client_errno(client)));

	    }

            break;

        }

        case AVAHI_BROWSER_REMOVE: {

	    if (flags & AVAHI_LOOKUP_RESULT_LOCAL) break;
            logoutput_info("service_browser_cb: remove %s %s %s", name, type, domain);

            /* send main message
        	howto translate this into ipv4, is this actual required? */

            break;

        }

        case AVAHI_BROWSER_FAILURE:

            logoutput_warning("service_browser_cb: service_browser failed: %s", avahi_strerror(avahi_client_errno(client)));
            break;

        case AVAHI_BROWSER_CACHE_EXHAUSTED:

            break;

        case AVAHI_BROWSER_ALL_FOR_NOW:

	    logoutput_info("service_browser_cb: ALL_FOR_NOW");
            break;

    }

}

static void browse_servicetype(const char *type, const char *domain)
{
    AvahiServiceBrowser *servicebrowser=NULL;
    AvahiStringList *i;

    for (i = types; i; i = i->next) {

        if (avahi_domain_equal(type, (char*) i->text)) return;

    }

    servicebrowser = avahi_service_browser_new(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, type, domain, 0, service_browser_cb, NULL);

    if (! servicebrowser) {

        logoutput_info("avahi_service_browser_new() failed: %s", avahi_strerror(avahi_client_errno(client)));
        return;
    }

    types = avahi_string_list_add(types, type);

}

static void browse_cb(AvahiServiceTypeBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *type, const char *domain, AVAHI_GCC_UNUSED AvahiLookupResultFlags flags, void* ptr)
{

    switch (event) {

        case AVAHI_BROWSER_FAILURE:

            logoutput_info("browse_cb: failure %s", avahi_strerror(avahi_client_errno(client)));
            return;

        case AVAHI_BROWSER_NEW:

            browse_servicetype(type, domain);

        case AVAHI_BROWSER_REMOVE:

            logoutput_info("browse_cb: remove type '%s' in domain '%s'", type, domain);
            break;

        case AVAHI_BROWSER_ALL_FOR_NOW:
        case AVAHI_BROWSER_CACHE_EXHAUSTED:

            logoutput_info("browse_cb %s", event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
            break;
    }

}

static void client_cb(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata)
{
    if (state == AVAHI_CLIENT_FAILURE) {
        logoutput_info("server connection failure: %s", avahi_strerror(avahi_client_errno(c)));
        // avahi_threaded_poll_quit(threadedpoll);
    }
}

struct AvahiWatch {
    struct bevent_s 		bevent;
    AvahiWatchCallback 		cb;
    AvahiWatchEvent		event;
    AvahiWatchEvent		last;
    void			*ptr;
};

static int bevent_callback(int fd, void *data, uint32_t bevent_events)
{
    AvahiWatch *watch=(AvahiWatch *) data;
    AvahiWatchEvent avahi_event;

    if (bevent_events & BEVENT_CODE_IN) avahi_event |= AVAHI_WATCH_IN;
    if (bevent_events & BEVENT_CODE_OUT) avahi_event |= AVAHI_WATCH_OUT;
    if (bevent_events & BEVENT_CODE_ERR) avahi_event |= AVAHI_WATCH_ERR;
    if (bevent_events & BEVENT_CODE_HUP) avahi_event |= AVAHI_WATCH_HUP;

    watch->last=avahi_event;
    (* watch->cb)(watch, fd, avahi_event, watch->ptr);
    return 0;

}

static AvahiWatch *new_AvahiWatch(const AvahiPoll *api, int fd, AvahiWatchEvent event, AvahiWatchCallback cb, void *ptr)
{
    AvahiWatch *watch=malloc(sizeof(AvahiWatch));

    if (watch) {
	uint32_t bevent_event=0;
	struct beventloop_s *loop=(struct beventloop_s *) api->userdata;

	init_bevent(&watch->bevent);
	set_bevent_name(&watch->bevent, "AVAHI");

	watch->event=event;
	watch->last=0;
	watch->cb=cb;
	watch->ptr=ptr; /* what points this to ?? */

	if (event & AVAHI_WATCH_IN) bevent_event |= BEVENT_CODE_IN;
	if (event & AVAHI_WATCH_OUT) bevent_event |= BEVENT_CODE_OUT;
	if (event & AVAHI_WATCH_ERR) bevent_event |= BEVENT_CODE_ERR;
	if (event & AVAHI_WATCH_HUP) bevent_event |= BEVENT_CODE_HUP;

	if (add_to_beventloop(fd, bevent_event, bevent_callback, (void *) watch, &(watch->bevent), loop)) {

	    logoutput("new_AvahiWatch: added fd %i to eventloop", fd);

	} else {

	    logoutput_warning("new_AvahiWatch: failed to add fd %i to eventloop", fd);
	    goto newfailed;

	}

	return watch;

    }

    newfailed:

    if (watch) {

	free(watch);
	watch=NULL;

    }

    return NULL;

}

static AvahiWatchEvent get_events_AvahiWatch(AvahiWatch *watch)
{
    return watch->last;
}

static void update_AvahiWatch(AvahiWatch *watch, AvahiWatchEvent event)
{
    if (watch->event != event) {
	uint32_t bevent_events=0;

	if (event & AVAHI_WATCH_IN) bevent_events |= BEVENT_CODE_IN;
	if (event & AVAHI_WATCH_OUT) bevent_events |= BEVENT_CODE_OUT;
	if (event & AVAHI_WATCH_ERR) bevent_events |= BEVENT_CODE_ERR;
	if (event & AVAHI_WATCH_HUP) bevent_events |= BEVENT_CODE_HUP;
	modify_bevent(&watch->bevent, bevent_events);

    }

}

static void free_AvahiWatch(AvahiWatch *watch)
{
    remove_bevent_from_beventloop(&watch->bevent);
    free(watch);
}

static void free_AvahiTimeout(AvahiTimeout *t)
{
}

static AvahiTimeout *new_AvahiTimeout(const AvahiPoll *api, const struct timeval *tv, AvahiTimeoutCallback cb, void *ptr)
{
    return NULL;
}

static void update_AvahiTimeout(AvahiTimeout *t, const struct timeval *tv)
{
}

void browse_services_avahi(void *userdata) {
    AvahiServiceTypeBrowser *browser = NULL;
    AvahiPoll api;
    int error;

    /* TODO:
	create a AvahiPoll api:
	- userdata points to main eventloop here 
	- watch_new created a watch on a fd using eventloop custom api
	- watch_update
	- watch_get_events
	- watch_free
    */

    api.userdata=userdata;
    api.watch_new=new_AvahiWatch;
    api.watch_update=update_AvahiWatch;
    api.watch_get_events=get_events_AvahiWatch;
    api.watch_free=free_AvahiWatch;
    api.timeout_new=new_AvahiTimeout;
    api.timeout_update=update_AvahiTimeout;
    api.timeout_free=free_AvahiTimeout;

    if (!(threadedpoll = avahi_threaded_poll_new())) {

	logoutput_warning("browse_services_avahi: failed to create simple poll object");
        goto fail;

    }

    client = avahi_client_new(avahi_threaded_poll_get(threadedpoll), 0, client_cb, NULL, &error);
    // client = avahi_client_new(&api, 0, client_cb, NULL, &error);

    if (! client) {

        logoutput_warning("browse_services_avahi: failed to create client: %s", avahi_strerror(error));
        goto fail;

    }

    browser = avahi_service_type_browser_new(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_INET, NULL, 0, browse_cb, NULL);

    if (! browser) {

        logoutput_info("browse_services_avahi: failed to create service browser: %s", avahi_strerror(avahi_client_errno(client)));
        goto fail;

    }

    logoutput_info("browse_services_avahi: start threaded poll");

    avahi_threaded_poll_start(threadedpoll);
    return;

fail:

    if (browser) {

	avahi_service_type_browser_free(browser);
	browser=NULL;
    }

    if (client) {

	avahi_client_free(client);
	client=NULL;

    }

    if (threadedpoll) {

	avahi_threaded_poll_free(threadedpoll);
	threadedpoll=NULL;

    }

}

void stop_browse_avahi()
{

    logoutput_info("stop_browse_avahi: stop poll");

    if (threadedpoll) {
	avahi_threaded_poll_lock(threadedpoll);
	avahi_threaded_poll_stop(threadedpoll);
	avahi_threaded_poll_unlock(threadedpoll);
    }

    logoutput_info("stop_browse_avahi: free client");
    if (client) {
	avahi_client_free(client);
	client=NULL;
    }

    logoutput_info("stop_browse_avahi: free types found");
    if (types) {
	avahi_string_list_free(types);
	types=NULL;
    }

    logoutput_info("stop_browse_avahi: free poll");
    if (threadedpoll) {

	avahi_threaded_poll_free(threadedpoll);
	threadedpoll=NULL;

    }
}
