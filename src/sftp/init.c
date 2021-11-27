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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>

#include "main.h"
#include "log.h"
#include "misc.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"

#include "attr.h"
//#include "attr/read-attr-v03.h"
//#include "attr/read-attr-v04.h"
//#include "attr/read-attr-v05.h"
//#include "attr/read-attr-v06.h"

#include "recv.h"
//#include "recv/recv-v03.h"
//#include "recv/recv-v04.h"
//#include "recv/recv-v05.h"
//#include "recv/recv-v06.h"

#include "send.h"
//#include "send/send-v03.h"
//#include "send/send-v04.h"
//#include "send/send-v05.h"
//#include "send/send-v06.h"

#include "datatypes/ssh-uint.h"

#include "time.h"
#include "usermapping.h"
#include "extensions.h"
#include "init.h"

static void read_default_features_v06(struct sftp_client_s *sftp, struct ssh_string_s *data)
{
    struct sftp_supported_s *supported=&sftp->supported;
    unsigned int pos=0;
    unsigned int size=data->len;
    char *buff=data->ptr;

    if (size < 32) {

	logoutput_warning("read_default_features_v06: data too small (size=%i)", size);
	return;

    }

    supported->version.v06.attribute_mask=get_uint32(&buff[pos]);
    pos+=4;
    supported->version.v06.attribute_bits=get_uint32(&buff[pos]);
    pos+=4;
    supported->version.v06.open_flags=get_uint32(&buff[pos]);
    pos+=4;
    supported->version.v06.access_mask=get_uint32(&buff[pos]);
    pos+=4;
    supported->version.v06.max_read_size=get_uint32(&buff[pos]);
    pos+=4;
    supported->version.v06.open_block_vector=get_uint16(&buff[pos]);
    pos+=2;
    supported->version.v06.block_vector=get_uint16(&buff[pos]);
    pos+=2;
    supported->version.v06.attrib_extension_count=get_uint32(&buff[pos]);
    pos+=4;

    logoutput_debug("read_default_features_v06: attr count %i", supported->version.v06.attrib_extension_count);

    for (unsigned int i=0; i<supported->version.v06.attrib_extension_count; i++) {

	if (pos<size) {
	    struct ssh_string_s attrib=SSH_STRING_INIT;

	    pos+=read_ssh_string(&buff[pos], size-pos, &attrib);

	    if (attrib.len>0 && attrib.ptr) {

		logoutput_debug("read_default_features_v06: (%i - %i) found attrib extension %.*s", i, supported->version.v06.attrib_extension_count, attrib.len, attrib.ptr);

	    } else {

		goto error;

	    }

	} else {

	    goto error;

	}

    }

    supported->version.v06.extension_count=get_uint32(&buff[pos]);
    pos+=4;

    logoutput_debug("read_default_features_v06: ext count %i", supported->version.v06.extension_count);

    for (unsigned int i=0; i<supported->version.v06.extension_count; i++) {

	if (pos<size) {
	    struct ssh_string_s name=SSH_STRING_INIT;

	    pos+=read_ssh_string(&buff[pos], size-pos, &name);
	    logoutput("read_default_features_v06: found server extension %.*s", name.len, name.ptr);

	    if (name.len>0 && name.ptr) {
		int result=add_protocol_extension_server(sftp, &name, NULL);

		if ((result&SFTP_EXTENSION_ADD_RESULT_EXIST)==0) logoutput("read_default_features_v06: extension %.*s not created", name.len, name.ptr);

	    } else {

		goto error;

	    }

	} else {

	    goto error;

	}

    }

    error:
    return;

}

/*	process extensions part of the init verson message
	well known extensions are:
	- supported (used at version 5)
	- supported2 (used at version 6
	- acl-supported
	- text-seek
	- versions, version-select
	- filename-charset, filename-translation-control
	- newline
	- vendor-id
	- md5-hash. md5-hash-handle
	- check-file-handle, check-file-name
	- space-available
	- home-directory
	- copy-file, copy-data
	- get-temp-folder, make-temp-folder
	- */

static void process_sftp_extension(struct sftp_client_s *sftp, struct ssh_string_s *name, struct ssh_string_s *data)
{

    if (compare_ssh_string(name, 'c', "newline")==0)  {

	logoutput("process_sftp_extension: received newline extension");

    } else if (compare_ssh_string(name, 'c', "supported")==0) {

	logoutput("process_sftp_extension: received supported extension");

    } else if (compare_ssh_string(name, 'c', "supported2")==0) {

	if (get_sftp_protocol_version(sftp)>=6) {

	    read_default_features_v06(sftp, data);

	} else {

	    logoutput("process_sftp_extension: ignoring received supported2 extension (sftp version is %i, supported2 is used in version 6");

	}

    } else {

	int result=add_protocol_extension_server(sftp, name, data);

	if (result==SFTP_EXTENSION_ADD_RESULT_EXIST) {

	    logoutput("process_sftp_extension: found server extension %.*s", name->len, name->ptr);

	} else {

	    logoutput("process_sftp_extension: extension %.*s not created", name->len, name->ptr);

	}

    }

}

/*

    for sftp init data looks like:

    - extension-pair			extension[0..n]

    where one extension has the form:
    - string name
    - string data

    TODO: specific extensions handlers per version

*/

int process_sftp_version(struct sftp_client_s *sftp, struct sftp_reply_s *reply)
{
    unsigned char *buffer=reply->response.init.buff;
    unsigned int size=reply->response.init.size;
    unsigned int len=0;
    unsigned int version=0;
    unsigned int pos=0;
    unsigned int name_len=0;
    unsigned int data_len=0;

    version=get_uint32((char *) &buffer[pos]);
    pos+=4;

    logoutput("process_sftp_version: received server sftp version %i", version);
    set_sftp_protocol_version(sftp, version);

    /* check there is enough space for 2 uint and minimal 1 name */

    while (pos + 9 < size) {
	struct ssh_string_s name=SSH_STRING_INIT;

	pos+=read_ssh_string((char *) &buffer[pos], size-pos, &name);

	if (name.ptr) {
	    struct ssh_string_s data=SSH_STRING_INIT;

	    logoutput_debug("process_sftp_version: found %.*s", name.len, name.ptr);
	    pos+=read_ssh_string((char *) &buffer[pos], size-pos, &data);
	    process_sftp_extension(sftp, &name, &data);

	}

    }

    return 0;

}

/*
    assign the sftp functions to use after version negotiation
    and the server has send the supported extensions

    - send
    - recv
    - extensions
    - attr

*/

int set_sftp_protocol(struct sftp_client_s *sftp)
{
    int result=-1;
    unsigned char version=get_sftp_protocol_version(sftp);

    logoutput("set_sftp_protocol: use protocol version %i", version);

    if (version >= 3 && version <= 6) {

	set_sftp_attr_version(sftp);
	set_sftp_recv_version(sftp);
	set_sftp_send_version(sftp);

	result=(int) version;

    } else {

	logoutput("set_sftp_protocol: version %i not supported", version);

    }

    return result;

}

unsigned char get_sftp_protocol_version(struct sftp_client_s *sftp)
{
    unsigned char version = (sftp) ? sftp->protocol.version : 0;

    /* TODO ... */

    return (unsigned char) ((version>0) ? version : 6);

}

void set_sftp_protocol_version(struct sftp_client_s *sftp, unsigned char version)
{
    sftp->protocol.version=version;
}
