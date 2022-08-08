/*

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
#include "libosns-network.h"
#include "libosns-misc.h"
#include "libosns-list.h"
#include "libosns-datatypes.h"
#include "libosns-lock.h"
#include "libosns-eventloop.h"
#include "libosns-mountinfo.h"

#include "osns-protocol.h"

#include "receive.h"
#include "query.h"
#include "query-filter.h"
#include "osns/record.h"

static const unsigned int allflags = (OSNS_MOUNTINFO_QUERY_FLAG_PSEUDOFS);
static const unsigned int allvalid = (OSNS_MOUNTINFO_QUERY_ATTR_FS | OSNS_MOUNTINFO_QUERY_ATTR_SOURCE | OSNS_MOUNTINFO_QUERY_ATTR_ROOT | OSNS_MOUNTINFO_QUERY_ATTR_PATH | OSNS_MOUNTINFO_QUERY_ATTR_DEV_MAJORMINOR | OSNS_MOUNTINFO_QUERY_ATTR_OPTIONS | OSNS_MOUNTINFO_QUERY_ATTR_FLAGS);
static const unsigned int default_valid = allvalid;

#define FIELD_TYPE_NAME_STRING				1

/* calculate the size */

static unsigned int write_mountinfo_buffer_size(struct mountentry_s *me, unsigned int valid, struct attr_buffer_s *ab, unsigned int *result)
{
    unsigned int pos=ab->pos;

    logoutput_debug("write_mountinfo_buffer_size");

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_FS) {
	unsigned char len=strlen(me->fs);

	(* ab->ops->rw.write.write_uchar)(ab, len);
	(* ab->ops->rw.write.write_uchars)(ab, (unsigned char *) me->fs, len);
	if (result) *result |= OSNS_MOUNTINFO_QUERY_ATTR_FS;

    }

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_SOURCE) {
	unsigned char len=strlen(me->source);

	(* ab->ops->rw.write.write_uchar)(ab, len);
	(* ab->ops->rw.write.write_uchars)(ab, (unsigned char *) me->source, len);
	if (result) *result |= OSNS_MOUNTINFO_QUERY_ATTR_SOURCE;

    }

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_ROOT) {
	unsigned char len=strlen(me->rootpath);

	(* ab->ops->rw.write.write_uint16)(ab, len);
	(* ab->ops->rw.write.write_uchars)(ab, (unsigned char *) me->rootpath, len);
	if (result) *result |= OSNS_MOUNTINFO_QUERY_ATTR_ROOT;

    }

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_PATH) {
	unsigned char len=strlen(me->mountpoint);

	(* ab->ops->rw.write.write_uint16)(ab, len);
	(* ab->ops->rw.write.write_uchars)(ab, (unsigned char *) me->mountpoint, len);
	if (result) *result |= OSNS_MOUNTINFO_QUERY_ATTR_PATH;

    }

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_DEV_MAJORMINOR) {
	uint32_t dev=makedev(me->major, me->minor);

	(* ab->ops->rw.write.write_uint32)(ab, dev);
	if (result) *result |= OSNS_MOUNTINFO_QUERY_ATTR_DEV_MAJORMINOR;

    }

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_OPTIONS) {
	unsigned char len=strlen(me->options);

	(* ab->ops->rw.write.write_uint16)(ab, len);
	(* ab->ops->rw.write.write_uchars)(ab, (unsigned char *) me->options, len);
	if (result) *result |= OSNS_MOUNTINFO_QUERY_ATTR_OPTIONS;

    }

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_FLAGS) {

	(* ab->ops->rw.write.write_uint32)(ab, me->flags);
	if (result) *result |= OSNS_MOUNTINFO_QUERY_ATTR_FLAGS;

    }

    return (ab->pos - pos);

}

static int cb_filter_custom(struct openquery_request_s *request, void *ptr)
{
    struct query_mountinfo_attr_s *filter=request->filter.mountinfo;
    struct mountentry_s *me=(struct mountentry_s *) ptr;
    unsigned int valid=filter->valid;
    int (* cb_compare_names)(struct name_string_s *name, char *value);
    int (* cb_compare_records)(struct osns_record_s *record, char *value);
    int (* cb_compare_uint32)(uint32_t param, uint32_t value);

    if (filter->flags & OSNS_MOUNTINFO_QUERY_FLAG_VALID_OR) {

	cb_compare_names=compare_filter_names_or;
	cb_compare_records=compare_filter_records_or;
	cb_compare_uint32=compare_filter_uint32_or;

    } else {

	cb_compare_names=compare_filter_names_and;
	cb_compare_records=compare_filter_records_and;
	cb_compare_uint32=compare_filter_uint32_and;

    }

    logoutput_debug("cb_filter_custom");

    /* if filter on createtime is specified alway do that the same */

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_CREATETIME) {

	if (request->flags & OSNS_MOUNTINFO_QUERY_FLAG_CREATETIME_AFTER) {

	    /* only those records allowed where createtime is after the time specified in the filter */
	    if (system_time_test_earlier(&filter->createtime, &me->created)<=0) return -1;

	} else {

	    if (system_time_test_earlier(&filter->createtime, &me->created)>0) return -1;

	}

	valid &= ~OSNS_MOUNTINFO_QUERY_ATTR_CREATETIME;

    }

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_FS) {
	int tmp=(* cb_compare_names)(&filter->fs, me->fs);

	if (tmp==-1 || tmp==1) return tmp;
	valid &= ~OSNS_MOUNTINFO_QUERY_ATTR_FS;

    }

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_SOURCE) {
	int tmp=(* cb_compare_names)(&filter->source, me->source);

	if (tmp==-1 || tmp==1) return tmp;
	valid &= ~OSNS_MOUNTINFO_QUERY_ATTR_SOURCE;

    }

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_ROOT) {
	int tmp=(* cb_compare_records)(&filter->root, me->rootpath);

	if (tmp==-1 || tmp==1) return tmp;
	valid &= ~OSNS_MOUNTINFO_QUERY_ATTR_ROOT;

    }

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_PATH) {
	int tmp=(* cb_compare_records)(&filter->path, me->mountpoint);

	if (tmp==-1 || tmp==1) return tmp;
	valid &= ~OSNS_MOUNTINFO_QUERY_ATTR_PATH;

    }

    if (filter->valid & OSNS_MOUNTINFO_QUERY_ATTR_DEV_MAJORMINOR) {
	uint32_t filterdev=(uint32_t) makedev(filter->major, filter->minor);
	uint32_t medev=(uint32_t) makedev(me->major, me->minor);
	int tmp=(* cb_compare_uint32)(filterdev, medev);

	if (tmp==-1 || tmp==1) return tmp;
	valid &= ~OSNS_NETCACHE_QUERY_ATTR_PORT;

    }

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_OPTIONS) {
	int tmp=(* cb_compare_records)(&filter->options, me->options);

	if (tmp==-1 || tmp==1) return tmp;
	valid &= ~OSNS_MOUNTINFO_QUERY_ATTR_SOURCE;

    }

    if (valid>0) {

	logoutput_warning("cb_filter_custom: not all required filters applied (valid=%u)", valid);

    }

    /* when here depends on the flag VALID_OR:
	- OR: no test has been a match: no success
	- AND: no test has been a reason to exclude: success */

    return ((filter->flags & OSNS_MOUNTINFO_QUERY_FLAG_VALID_OR) ? -1 : 1);

}

static struct mountentry_s *get_next_mountentry(struct mount_monitor_s *monitor, struct mountentry_s *me)
{
    unsigned int offset=me->mountid;

    release_mountentry(me);
    return find_mountentry(monitor, offset+1, 0);
}

static unsigned int cb_read_mountinfo(struct openquery_request_s *request, struct readquery_request_s *readquery)
{
    struct mount_monitor_s *monitor=get_default_mount_monitor();
    struct query_mountinfo_attr_s *filter=request->filter.mountinfo;
    struct mountentry_s *me=find_mountentry(monitor, readquery->offset, 0);
    unsigned int pos=0;

    if (request->valid==0) {

	request->valid=default_valid;
	logoutput_debug("cb_read_mountinfo: taking default valid %i", request->valid);

    } else {

	logoutput_debug("cb_read_mountinfo: client specified valid %i", request->valid);

    }

    while (me) {

	if ((* request->cb_filter)(request, (void *) me)==1) {
	    struct attr_buffer_s ab=INIT_ATTR_BUFFER_NOWRITE;
	    unsigned int valid=0;
	    unsigned int tmp=0;
	    char *buffer=readquery->buffer;

	    set_attr_buffer_nowrite(&ab);

	    /* write record to buffer ... test it fits  */

	    logoutput_debug("cb_read_mountinfo: pos %u size %u", pos, readquery->size);

	    /* get required buffer size */

	    tmp=(8 + write_mountinfo_buffer_size(me, request->valid, &ab, &valid));

	    if (tmp > (readquery->size - pos)) {

		logoutput_debug("cb_read_mouninfo: no space in buffer for next record (%u)", tmp);
		break;

	    }

	    set_attr_buffer_write(&ab, &buffer[pos], (readquery->size-pos));

	    /* form of record over the connection is:
	    - uint32			offset
	    - uint32			valid
	    - ATTR			attr
	    */

	    logoutput_debug("cb_read_mountinfo: offset %u valid %u", me->mountid, valid);

	    (* ab.ops->rw.write.write_uint32)(&ab, me->mountid);
	    (* ab.ops->rw.write.write_uint32)(&ab, valid);
	    tmp=write_mountinfo_buffer_size(me, valid, &ab, NULL);

	    logoutput_debug("cb_read_mountinfo: ab.pos %u", ab.pos);

	    pos+=ab.pos;
	    readquery->count++;

	}

	logoutput_debug("cb_read_mountinfo: next");
	me=get_next_mountentry(monitor, me);

    }

    logoutput_debug("cb_read_mountinfo: pos %u", pos);
    return pos;

}

static void cb_close_mountinfo(struct openquery_request_s *request)
{

    if (request->filter.mountinfo) {

	free(request->filter.mountinfo);
	request->filter.mountinfo=NULL;

    }

}

static int read_mountinfo_fields(unsigned int valid, char *data, unsigned int size, struct openquery_request_s *request)
{
    struct query_mountinfo_attr_s *filter=request->filter.mountinfo;
    unsigned int pos=0;

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_FS) {
	struct name_string_s name=NAME_STRING_INIT;
	unsigned int len=0;

	len=read_name_string(&data[pos], (size-pos), &name);
	if (len<=1) return -1;
	pos+=len;

	if (filter) {
	    char *buffer=filter->buffer;
	    unsigned int tmp=0;

	    tmp=write_name_string(&buffer[filter->pos], (filter->size - filter->pos), 'n', &name);
	    tmp=read_name_string(&buffer[filter->pos], (filter->size - filter->pos), &filter->fs);

	    filter->pos+=tmp;
	    filter->valid |= OSNS_MOUNTINFO_QUERY_ATTR_FS;

	}

	valid &= ~OSNS_MOUNTINFO_QUERY_ATTR_FS;

    }

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_SOURCE) {
	struct name_string_s name=NAME_STRING_INIT;
	unsigned int len=0;

	len=read_name_string(&data[pos], (size-pos), &name);
	if (len<=1) return -1;
	pos+=len;

	if (filter) {
	    char *buffer=filter->buffer;
	    unsigned int tmp=0;

	    tmp=write_name_string(&buffer[filter->pos], (filter->size - filter->pos), 'n', &name);
	    tmp=read_name_string(&buffer[filter->pos], (filter->size - filter->pos), &filter->source);

	    filter->pos+=tmp;
	    filter->valid |= OSNS_MOUNTINFO_QUERY_ATTR_SOURCE;

	}

	valid &= ~OSNS_MOUNTINFO_QUERY_ATTR_SOURCE;

    }

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_ROOT) {
	struct osns_record_s root={0, NULL};
	unsigned int len=0;

	len=read_osns_record(&data[pos], (size-pos), &root);
	if (len<=2) return -1;
	pos+=len;

	if (filter) {
	    char *buffer=filter->buffer;
	    unsigned int tmp=0;

	    tmp=write_osns_record(&buffer[filter->pos], (filter->size - filter->pos), 'r', &root);
	    tmp=read_osns_record(&buffer[filter->pos], (filter->size - filter->pos), &filter->root);

	    filter->pos+=tmp;
	    filter->valid |= OSNS_MOUNTINFO_QUERY_ATTR_ROOT;

	}

	valid &= ~OSNS_MOUNTINFO_QUERY_ATTR_ROOT;

    }

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_PATH) {
	struct osns_record_s path={0, NULL};
	unsigned int len=0;

	len=read_osns_record(&data[pos], (size-pos), &path);
	if (len<=2) return -1;
	pos+=len;

	if (filter) {
	    char *buffer=filter->buffer;
	    unsigned int tmp=0;

	    tmp=write_osns_record(&buffer[filter->pos], (filter->size - filter->pos), 'r', &path);
	    tmp=read_osns_record(&buffer[filter->pos], (filter->size - filter->pos), &filter->path);

	    filter->pos+=tmp;
	    filter->valid |= OSNS_MOUNTINFO_QUERY_ATTR_PATH;

	}

	valid &= ~OSNS_MOUNTINFO_QUERY_ATTR_PATH;

    }

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_DEV_MAJORMINOR) {
	uint32_t devmajorminor=get_uint32(&data[pos]);

	pos+=4;

	if (filter) {

	    filter->major=major(devmajorminor);
	    filter->minor=minor(devmajorminor);
	    filter->valid |= OSNS_MOUNTINFO_QUERY_ATTR_DEV_MAJORMINOR;

	}

	valid &= ~OSNS_MOUNTINFO_QUERY_ATTR_DEV_MAJORMINOR;

    }

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_OPTIONS) {
	struct osns_record_s options={0, NULL};
	unsigned int len=0;

	len=read_osns_record(&data[pos], (size-pos), &options);
	if (len<=2) return -1;
	pos+=len;

	if (filter) {
	    char *buffer=filter->buffer;
	    unsigned int tmp=0;

	    tmp=write_osns_record(&buffer[filter->pos], (filter->size - filter->pos), 'r', &options);
	    tmp=read_osns_record(&buffer[filter->pos], (filter->size - filter->pos), &filter->options);

	    filter->pos+=tmp;
	    filter->valid |= OSNS_MOUNTINFO_QUERY_ATTR_OPTIONS;

	}

	valid &= ~OSNS_MOUNTINFO_QUERY_ATTR_OPTIONS;

    }

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_FLAGS) {
	uint32_t flags=get_uint32(&data[pos]);

	pos+=4;

	if (filter) {

	    filter->flags=flags;
	    filter->valid |= OSNS_MOUNTINFO_QUERY_ATTR_FLAGS;

	}

	valid &= ~OSNS_MOUNTINFO_QUERY_ATTR_FLAGS;

    }

    if (valid & OSNS_MOUNTINFO_QUERY_ATTR_CREATETIME) {
	int64_t seconds=0;
	uint32_t nseconds=0;

	seconds=get_int64(&data[pos]);
	pos+=8;
	nseconds=get_uint32(&data[pos]);
	pos+=4;

	if (filter) {

	    set_system_time(&filter->createtime, seconds, nseconds);
	    filter->valid|=OSNS_MOUNTINFO_QUERY_ATTR_CREATETIME;

	}

	valid &= ~OSNS_NETCACHE_QUERY_ATTR_CREATETIME;

    }

    if (valid>0) {

	logoutput_warning("read_mountinfo_fields: valid %i", valid);

    }

    return (int) pos;
}

static int read_mountinfo_query_filter(char *data, unsigned int size, struct openquery_request_s *request)
{
    struct query_mountinfo_attr_s *filter=NULL;
    unsigned int pos=0;
    unsigned int valid=0;
    unsigned int flags=0;
    int len=0;

    flags=get_uint32(&data[pos]);
    pos+=4;
    valid=get_uint32(&data[pos]);
    pos+=4;
    logoutput_debug("read_mountinfo_query_filter: flags %i valid %u", flags, valid);
    if (valid==0) return 0;

    /* read the name values to determine the required size */

    len=read_mountinfo_fields(valid, &data[pos], size-pos, request);

    if (len==-1) {

	logoutput_debug("read_mountinfo_query_filter: error reading fields");
	goto error;

    }

    logoutput_debug("read_mountinfo_query_filter: len %i", len);

    filter=malloc(sizeof(struct query_mountinfo_attr_s) + len);
    if (filter==NULL) goto error;
    memset(filter, 0, sizeof(struct query_mountinfo_attr_s) + len);
    filter->size = (unsigned int) len;
    filter->flags = (flags & OSNS_MOUNTINFO_QUERY_FLAG_VALID_OR);

    request->filter.mountinfo=filter;
    request->cb_filter=cb_filter_custom;

    /* read the name values and put them in buffer */

    len=read_mountinfo_fields(valid, &data[pos], size-pos, request);
    pos+=(unsigned int) len;

    return (int) pos;
    error:
    return -1;

}

/*
    data looks like:
    uint32				flags
    uint32				valid
    record string
	QUERYATTR			attr to filter the result
*/

int process_openquery_mountinfo(struct osns_receive_s *r, uint32_t id, char *data, unsigned int size, struct openquery_request_s *request)
{
    struct query_netcache_attr_s *filter=NULL;
    unsigned int flags=0;
    unsigned int len=0;
    unsigned int valid=0;
    unsigned int pos=0;

    logoutput_debug("process_openquery_mountinfo: id %u size %u", id, size);
    /* the filter should not be defined here ... */
    if (request->filter.mountinfo) return OSNS_STATUS_SYSTEMERROR;

    /* prepare a query in netcache
	filter is in data */

    if (size<10) return OSNS_STATUS_PROTOCOLERROR;
    flags=get_uint32(&data[pos]);
    pos+=4;
    valid=get_uint32(&data[pos]);
    pos+=4;
    len=get_uint16(&data[pos]); /* length of filter record */
    pos+=2;

    request->flags = (flags & allflags);
    request->valid = (valid & allvalid);

    if (request->valid != valid) {

	logoutput_warning("process_openquery_mountinfo: not all valid flags reckognized (%i, %i supported)", valid, request->valid);

    }

    logoutput_debug("process_openquery_mountinfo: len %u valid %u flags %u", len, valid, flags);

    if (len==0) {

	/* no filter specified */

	if (request->flags & OSNS_NETCACHE_QUERY_FLAG_CREATETIME_AFTER) {

	    /* flags are invalid since no createtime/chamgetime is specified ... */

	    logoutput_warning("process_openquery_mountinfo: error flags createtime/changetime after specified but no filter data");
	    return OSNS_STATUS_INVALIDFLAGS;

	}

    } else {

	if (10 + len > size) return OSNS_STATUS_PROTOCOLERROR;
	if (read_mountinfo_query_filter(&data[pos], len, request)==-1) {

	    if (request->filter.mountinfo) {

		free(request->filter.mountinfo);
		request->filter.mountinfo=NULL;

	    }

	    logoutput_warning("process_openquery_mountinfo: error reading/processing data/filter");
	    return OSNS_STATUS_PROTOCOLERROR;

	}

    }

    request->cb_read=cb_read_mountinfo;
    request->cb_close=cb_close_mountinfo;
    return 0;

}
