/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017, 2018 Stef Bon <stefbon@gmail.com>

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

#include "libosns-misc.h"
#include "libosns-log.h"

#include "ssh-common.h"
#include "ssh-utils.h"
#include "ssh-connections.h"


extern struct fs_options_s fs_options;

static struct ssh_extension_s available_extensions[] = {
    { .name = "server-sig-algs", .code = SSH_EXTENSION_SERVER_SIG_ALGS},
    { .name = "delay-compression", .code = SSH_EXTENSION_DELAY_COMPRESSION},
    { .name = "no-flow-control", .code = SSH_EXTENSION_NO_FLOW_CONTROL},
    { .name = "elevation", .code = SSH_EXTENSION_ELEVATION},
    { .name = "global_request:supported", .code = SSH_EXTENSION_GR_SUPPORT},
    { .name = NULL, .code=0}
};

void init_ssh_extensions(struct ssh_session_s *session)
{
    struct ssh_extensions_s *extensions=&session->extensions;
    extensions->supported=0;
    extensions->received=0;
}

const char *get_extension_name(unsigned int code)
{
    unsigned int i=0;
    char *name=NULL;

    while (available_extensions[i].code>0) {

	if (available_extensions[i].code == code) {

	    name=available_extensions[i].name;
	    break;

	}

	i++;

    }

    return name;

}

unsigned int find_extension_code(struct ssh_string_s *name)
{
    unsigned int i=0;
    unsigned int code=0;

    while (available_extensions[i].code>0) {

	if (strlen(available_extensions[i].name)==name->len && strncmp(name->ptr, available_extensions[i].name, name->len) == 0) {

	    code=available_extensions[i].code;
	    break;

	}

	i++;

    }

    return code;

}

void process_server_sig_algs(struct ssh_session_s *session, struct ssh_string_s *data)
{
    char *name=NULL;
    char *sep=NULL;
    char search[data->len + 2];
    int left=data->len + 1;

    /* put one comma at end: that eases the looking for names in this comma seperated list */

    memcpy(search, data->ptr, data->len);
    search[data->len]=',';
    search[data->len+1]='\0';

    name=search;

    getname:

    sep=memchr(name, ',', left);
    if (sep) {
	struct ssh_pkalgo_s *algo=NULL;
	int index=-1;

	*sep='\0';
	logoutput("process_server_sig_algs: received %s", name);

	/* lookup the signalgo name in the available sign algos */

	algo=get_pkalgo(name, strlen(name), &index);

	if (algo && index>=0) {

	    logoutput("process_server_sig_algs: algo %s supported (index=%i)", name, index);
	    enable_algo_pubkey(session, index, "server", "pksign");

	}

	*sep=',';
	left -= (sep + 1 - name);
	name=sep+1;
	if (left>0) goto getname;

    }

}

void process_delay_compression(struct ssh_session_s *session, struct ssh_string_s *data)
{
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;
    struct ssh_string_s compr_c2s;
    struct ssh_string_s compr_s2c;
    unsigned int index=SSH_EXTENSION_DELAY_COMPRESSION - 1;

    session->config.extensions|=(1 << ( 2 * index + 1));
    if ((session->config.extensions & (1 << ( 2 * index)))==0) return; /* delay compression not supported from config */

    set_msg_buffer_string(&mb, data);
    init_ssh_string(&compr_c2s);
    init_ssh_string(&compr_s2c);

    msg_read_ssh_string(&mb, &compr_c2s);
    msg_read_ssh_string(&mb, &compr_s2c);

    if (mb.error==0) {

	logoutput("process_delay_compression: received compr_c2s %.*s", compr_c2s.len, compr_c2s.ptr);
	logoutput("process_delay_compression: received compr_s2c %.*s", compr_s2c.len, compr_s2c.ptr);

    } else {

	logoutput("process_delay_compression: error %i reading data", mb.error);

    }

}

void process_no_flow_control(struct ssh_session_s *session, struct ssh_string_s *data)
{
    logoutput("process_no_flow_control: received choice %.*s", data->len, data->ptr);
}

void process_elevation(struct ssh_session_s *session, struct ssh_string_s *data)
{
    logoutput("process_elevation: received choice %.*s", data->len, data->ptr);
}

void process_gr_support(struct ssh_session_s *session, struct ssh_string_s *data)
{
    logoutput("process_gr_support: received %.*s", data->len, data->ptr);
}

void process_msg_ext_info(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    struct ssh_session_s *session=get_ssh_connection_session(connection);
    struct ssh_extensions_s *extensions=&session->extensions;
    unsigned int count=0;
    struct msg_buffer_s mb = INIT_SSH_MSG_BUFFER;
    unsigned int received=0;

    set_msg_buffer_payload(&mb, payload);

    msg_read_byte(&mb, NULL);
    msg_read_uint32(&mb, &count);

    /* check it's valid, is there a sane maximum? */

    if (count>0) {

	for (unsigned int i=0; i<count; i++) {
	    struct ssh_string_s name=SSH_STRING_INIT;
	    struct ssh_string_s data=SSH_STRING_INIT;
	    int code=0;

	    msg_read_ssh_string(&mb, &name);
	    msg_read_ssh_string(&mb, &data);

	    code=find_extension_code(&name);

	    switch (code) {

	    case SSH_EXTENSION_SERVER_SIG_ALGS:

		process_server_sig_algs(session, &data);
		break;

	    case SSH_EXTENSION_DELAY_COMPRESSION:

		process_delay_compression(session, &data);
		break;

	    case SSH_EXTENSION_NO_FLOW_CONTROL:

		process_no_flow_control(session, &data);
		break;

	    case SSH_EXTENSION_ELEVATION:

		process_no_flow_control(session, &data);
		break;

	    case SSH_EXTENSION_GR_SUPPORT:

		process_gr_support(session, &data);
		break;

	    default:

		logoutput("process_msg_ext_info: extension %.*s not reckognized", name.len, name.ptr);

	    }

	}

    }

}
