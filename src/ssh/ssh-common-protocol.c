/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#include "ssh-common-protocol.h"

extern void store_uint32(char *buff, uint32_t value);

struct disconnect_reasons_s {
    unsigned int 	reason;
    const char		*description;
};

static struct disconnect_reasons_s d_reasons[] = {
    {0							, ""},
    {SSH_DISCONNECT_HOST_NOT_ALLOWED_TO_CONNECT		, "Host is not allowed to connect."},
    {SSH_DISCONNECT_PROTOCOL_ERROR			, "Protocol error."},
    {SSH_DISCONNECT_KEY_EXCHANGE_FAILED			, "Key exchange failed."},
    {SSH_DISCONNECT_RESERVED				, "Reserved."},
    {SSH_DISCONNECT_MAC_ERROR				, "MAC error,"},
    {SSH_DISCONNECT_COMPRESSION_ERROR			, "Compression error."},
    {SSH_DISCONNECT_SERVICE_NOT_AVAILABLE		, "Service not available."},
    {SSH_DISCONNECT_PROTOCOL_VERSION_NOT_SUPPORTED	, "Protocol version is not supported."},
    {SSH_DISCONNECT_HOST_KEY_NOT_VERIFIABLE		, "Host key is not verifiable."},
    {SSH_DISCONNECT_CONNECTION_LOST			, "Connection is lost."},
    {SSH_DISCONNECT_BY_APPLICATION			, "Disconnected by application."},
    {SSH_DISCONNECT_TOO_MANY_CONNECTIONS		, "Too many connections."},
    {SSH_DISCONNECT_AUTH_CANCELLED_BY_USER		, "Authorization cancelled by user."},
    {SSH_DISCONNECT_NO_MORE_AUTH_METHODS_AVAILABLE	, "No more authorization methods are available."},
    {SSH_DISCONNECT_ILLEGAL_USER_NAME			, "Illegal user name."}};

const char *get_disconnect_reason(unsigned int reason)
{
    if (reason>=0 && reason <= SSH_DISCONNECT_ILLEGAL_USER_NAME) return d_reasons[reason].description;
    return NULL;
}
