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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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

#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>

#include "misc.h"
#include "eventloop.h"

static AvahiClient *client = NULL;
static AvahiStringList *types = NULL;
static void (* add_net_service_cb)(const char *name, const char *hostname, char *ipv4, const char *domain, unsigned int port, const char *type);

static void service_resolver_cb(AvahiServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event,
				const char *name, const char *type, const char *domain, const char *hostname, const AvahiAddress *a, uint16_t port,
				AvahiStringList *txt, AVAHI_GCC_UNUSED AvahiLookupResultFlags flags, void *userdata)
{

    switch (event) {

        case AVAHI_RESOLVER_FOUND: {

	    if (a->proto==AVAHI_PROTO_INET) {
		char ipv4[AVAHI_ADDRESS_STR_MAX+1];

		memset(ipv4, 0, AVAHI_ADDRESS_STR_MAX+1);
		avahi_address_snprint(ipv4, AVAHI_ADDRESS_STR_MAX, a);

		if (strlen(ipv4)>0) {

		    logoutput("service_resolver_cb: found ipv4 %s for name %s domain %s hostname %s port %i", ipv4, name, domain, hostname, (int) port);

		    ( * add_net_service_cb)(name, hostname, ipv4, domain, port, type);

		}

	    }

            break;
        }

        case AVAHI_RESOLVER_FAILURE:

            logoutput_warning("service_resolver_cb: failed to resolve service '%s' of type '%s' in domain '%s': %s", name, type, domain, avahi_strerror(avahi_client_errno(client)));
            break;
    }

    if (r) avahi_service_resolver_free(r);

}

static void service_browser_cb(AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name, const char *type, const char *domain, AvahiLookupResultFlags flags, void *ptr)
{

    switch (event) {

        case AVAHI_BROWSER_NEW: {

            if (flags & AVAHI_LOOKUP_RESULT_LOCAL) break;
            logoutput("service_browser_cb: new %s %s %s", name, type, domain);

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

static void cb_default(const char *name, const char *hostname, char *ipv4, const char *domain, unsigned int port, const char *type)
{
    logoutput("cb_default: name %s host %s ipv4 %s domain %s port %i type %s", name, hostname, ipv4, domain, port, type);
}

void browse_services_avahi(void (* cb)(const char *name, const char *hostname, char *ipv4, const char *domain, unsigned int port, const char *type)) {
    AvahiServiceTypeBrowser *browser = NULL;
    AvahiPoll *poll_api=NULL;
    AvahiGLibPoll *glib_poll=NULL;
    int error;

    avahi_set_allocator(avahi_glib_allocator());

    if (cb==NULL) {

	add_net_service_cb=cb_default;

    } else {

	add_net_service_cb=cb;

    }

    glib_poll = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);
    poll_api = avahi_glib_poll_get(glib_poll);
    client = avahi_client_new(poll_api, 0, client_cb, NULL, &error);

    if (! client) {

        logoutput_warning("browse_services_avahi: failed to create client: %s", avahi_strerror(error));
        goto fail;

    }

    logoutput("browse_services_avahi: new client");

    browser = avahi_service_type_browser_new(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_INET, NULL, 0, browse_cb, NULL);

    if (! browser) {

        logoutput_info("browse_services_avahi: failed to create service browser: %s", avahi_strerror(avahi_client_errno(client)));
        goto fail;

    }

    logoutput("browse_services_avahi: new service browser");

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

}

void stop_browse_avahi()
{

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

}

