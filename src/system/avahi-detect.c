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
#include "libosns-list.h"
#include "libosns-system.h"

#include "osns-protocol.h"

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

#include <avahi-common/cdecl.h>
#include <avahi-common/thread-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/domain.h>

#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>

#include "netcache.h"

static AvahiClient *client = NULL;
static AvahiStringList *types = NULL;
static unsigned int AvahiGlobalFlags=0;

static void service_resolver_cb(AvahiServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event,
				const char *name, const char *type, const char *domain, const char *hostname, const AvahiAddress *a, uint16_t port,
				AvahiStringList *txt, AVAHI_GCC_UNUSED AvahiLookupResultFlags flags, void *userdata)
{

    switch (event) {

        case AVAHI_RESOLVER_FOUND: {
	    char tmp[AVAHI_ADDRESS_STR_MAX+1];
	    unsigned char ip_family=0;

	    memset(tmp, 0, AVAHI_ADDRESS_STR_MAX+1);
	    avahi_address_snprint(tmp, AVAHI_ADDRESS_STR_MAX, a);

	    if (a->proto==AVAHI_PROTO_INET) {

		ip_family=IP_ADDRESS_FAMILY_IPv4;

	    } else if (a->proto==AVAHI_PROTO_INET6) {

		ip_family=IP_ADDRESS_FAMILY_IPv6;

	    } else {

		if (check_family_ip_address(tmp, "ipv4")) {

		    ip_family=IP_ADDRESS_FAMILY_IPv4;

		} else if (check_family_ip_address(tmp, "ipv6")) {

		    ip_family=IP_ADDRESS_FAMILY_IPv6;

		}

	    }

	    if (ip_family==0 || strlen(tmp)==0) {

		logoutput("service_resolver_cb: not received enough information about ip address");
		break;

	    } else {
		unsigned int tmpflags=OSNS_NETCACHE_QUERY_FLAG_DNSSD;
		const char *tmpname=(name) ? name : hostname;

		if (flags & AVAHI_LOOKUP_RESULT_LOCAL) tmpflags |= OSNS_NETCACHE_QUERY_FLAG_LOCALHOST;

		logoutput_debug("service_resolver_cb: name %s hostname %s domain %s address %s", name, hostname, domain, tmp);

		add_detected_network_service(tmpname, NULL, tmp, ip_family, port, tmpflags, type);

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

	    if (! avahi_service_resolver_new(client, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, 0, service_resolver_cb, NULL)) {

		logoutput_warning("service_browser_cb: failed to resolve %s", name, domain, avahi_strerror(avahi_client_errno(client)));

	    } else {

		logoutput_debug("service_browser_cb: new %s %s %s", name, type, domain);

	    }

            break;

        }

        case AVAHI_BROWSER_REMOVE: {

            logoutput_debug("service_browser_cb: remove %s %s %s", name, type, domain);

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

        logoutput_warning("avahi_service_browser_new() failed: %s", avahi_strerror(avahi_client_errno(client)));
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

            logoutput_debug("browse_cb: remove type '%s' in domain '%s'", type, domain);
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
    }
}

void browse_services_avahi(unsigned int flags)
{
    AvahiGLibPoll *gpoll=NULL;
    AvahiServiceTypeBrowser *browser = NULL;
    const AvahiPoll *apoll=NULL;
    int error=0;

    logoutput("browse_services_avahi");

    if (client) {

	logoutput_warning("browse_services_avahi: client already created");
	return;

    }

    AvahiGlobalFlags|=flags;
    avahi_set_allocator(avahi_glib_allocator());

    gpoll=avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);
    apoll=avahi_glib_poll_get(gpoll);

    client = avahi_client_new(apoll, 0, client_cb, NULL, &error);

    if (! client) {

        logoutput_warning("browse_services_avahi: failed to create client: %s", avahi_strerror(error));
        goto out;

    }

    browser = avahi_service_type_browser_new(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_INET, NULL, 0, browse_cb, NULL);

    if (! browser) {

        logoutput_info("browse_services_avahi: failed to create service browser: %s", avahi_strerror(avahi_client_errno(client)));
        goto out;

    }

    logoutput("browse_services_avahi: new service browser");
    return;

    out:

    if (browser) avahi_service_type_browser_free(browser);
    if (client) avahi_client_free(client);

}

void stop_services_avahi()
{

    if (client) {

	avahi_client_free(client);
	client=NULL;

    }

}
