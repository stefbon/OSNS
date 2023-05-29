/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017, 2018, 2019, 2020 Stef Bon <stefbon@gmail.com>

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

#include "libosns-network.h"
#include "libosns-users.h"

#include "common-protocol.h"
#include "attr-context.h"

#include "attr-read.h"
#include "attr-write.h"
#include "attr-init.h"

static char *empty_name="";

static unsigned int get_maxlength_filename(struct attr_context_s *ctx)
{
    return 256;
}

static unsigned int get_maxlength_username(struct attr_context_s *ctx)
{
    return 32;
}

static unsigned int get_maxlength_groupname(struct attr_context_s *ctx)
{
    return 32;
}

static unsigned int get_maxlength_domainname(struct attr_context_s *ctx)
{
    return HOST_HOSTNAME_FQDN_MAX_LENGTH;
}

static unsigned char get_sftp_protocol_version(struct attr_context_s *ctx)
{
    logoutput_debug("get_sftp_protocol_version: you should not see this");
    return 0;
}

static unsigned int get_flags(struct attr_context_s *ctx, const char *what)
{
    return 0;
}

static void rw_attr_cb_zero(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat)
{}

void init_attrcb_zero(struct _rw_attrcb_s *attrcb, unsigned int count)
{

    for (unsigned int i=0; i<count; i++) {

	attrcb[i].code				= 0;
	attrcb[i].shift				= 0;
	attrcb[i].stat_mask			= 0;
	attrcb[i].fattr				= 0;
	attrcb[i].r_cb				= rw_attr_cb_zero;
	attrcb[i].w_cb				= rw_attr_cb_zero;
	attrcb[i].maxlength			= 0;
	attrcb[i].name				= empty_name;

    }

}

void init_sftp_valid(struct sftp_valid_s *valid)
{
    valid->mask=0;
    valid->flags=0;
}

void init_attr_context(struct attr_context_s *actx, unsigned int flags, void *ptr, struct net_idmapping_s *mapping)
{
    struct _rw_attrcb_s *attrcb=actx->attrcb;

    actx->flags=flags;
    actx->ptr=ptr;
    actx->mapping=mapping;

    init_sftp_valid(&actx->w_valid);
    init_sftp_valid(&actx->r_valid);

    init_attrcb_zero(attrcb, ATTR_CONTEXT_COUNT_ATTR_CB);

    actx->maxlength_filename=get_maxlength_filename;
    actx->maxlength_username=get_maxlength_username;
    actx->maxlength_groupname=get_maxlength_groupname;
    actx->maxlength_domainname=get_maxlength_domainname;
    actx->get_sftp_protocol_version=get_sftp_protocol_version;
    actx->get_sftp_flags=get_flags;

}

/* does nothing, required for initialization */
void parse_dummy(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat, unsigned char ctr)
{
}

struct sftp_valid_s *get_supported_valid_flags(struct attr_context_s *actx, unsigned char what)
{

    if (what=='r') {

	return &actx->r_valid;

    } else if (what=='w') {

	return &actx->w_valid;

    }

    return NULL;

}

void convert_sftp_valid_w(struct attr_context_s *actx, struct sftp_valid_s *valid, uint32_t bits)
{
    valid->mask = (bits & actx->w_valid.mask);
    valid->flags = (bits & actx->w_valid.flags);
}

void convert_sftp_valid_r(struct attr_context_s *actx, struct sftp_valid_s *valid, uint32_t bits)
{
    valid->mask = (bits & actx->r_valid.mask);
    valid->flags = (bits & actx->r_valid.flags);
}

void set_sftp_attr_context(struct attr_context_s *actx)
{
    unsigned char version=(* actx->get_sftp_protocol_version)(actx);
    struct attr_ops_s *ops=&actx->ops;

    logoutput_debug("set_sftp_attr_context: version %i", version);

    if (version<=3) {

	ops->read_name_name_response		= read_name_name_response_v03;
	ops->write_name_name_response		= write_name_name_response_v03;
	ops->parse_attributes			= parse_attributes_v03;
	ops->enable_attr			= enable_attr_v03;
	ops->get_property			= get_property_v03;
	init_attr_context_v03(actx);

    } else if (version==4) {

	ops->read_name_name_response		= read_name_name_response_v04;
	ops->write_name_name_response		= write_name_name_response_v04;
	ops->parse_attributes			= parse_attributes_v04;
	ops->enable_attr			= enable_attr_v04;
	ops->get_property			= get_property_v04;
	init_attr_context_v04(actx);

    } else if (version==5) {

	ops->read_name_name_response		= read_name_name_response_v04;
	ops->write_name_name_response		= write_name_name_response_v04;
	ops->parse_attributes			= parse_attributes_v05;
	ops->enable_attr			= enable_attr_v05;
	ops->get_property			= get_property_v05;
	init_attr_context_v05(actx);

    } else if (version==6) {

	ops->read_name_name_response		= read_name_name_response_v04;
	ops->write_name_name_response		= write_name_name_response_v04;
	ops->parse_attributes			= parse_attributes_v06;
	ops->enable_attr			= enable_attr_v06;
	ops->get_property			= get_property_v06;
	init_attr_context_v06(actx);

    } else {

	logoutput_warning("set_sftp_attr_context: error sftp protocol version %i not supported", version);

    }

}
