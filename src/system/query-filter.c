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

#include "osns-protocol.h"
#include "receive.h"
#include "query.h"
#include "osns/record.h"

int compare_filter_names_and(struct name_string_s *name, char *value)
{
    int tmp=compare_name_string(name, 'c', value);
    return ((tmp==0) ? 0 : -1);
}

int compare_filter_records_and(struct osns_record_s *record, char *value)
{
    int tmp=compare_osns_record(record, 'c', value);
    return ((tmp==0) ? 0 : -1);
}

int compare_filter_names_or(struct name_string_s *name, char *value)
{
    int tmp=compare_name_string(name, 'c', value);
    return ((tmp==0) ? 1 : 0);
}

int compare_filter_records_or(struct osns_record_s *record, char *value)
{
    int tmp=compare_osns_record(record, 'c', value);
    return ((tmp==0) ? 1 : 0);
}

int compare_filter_uint32_and(uint32_t param, uint32_t value)
{
    return ((param==value) ? 0 : -1);
}

int compare_filter_uint32_or(uint32_t param, uint32_t value)
{
    return ((param==value) ? 1 : 0);
}

