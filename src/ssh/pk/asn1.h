/*
  2018 Stef Bon <stefbon@gmail.com>

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

#ifndef SSH_PK_PK_ASN1_H
#define SSH_PK_PK_ASN1_H

#define _ASN1_TAG_SEQUENCE		0x30
#define _ASN1_TAG_INTEGER		0x02

struct asn1_tlv_s {
    unsigned char			tag;
    unsigned int			len;
    unsigned int 			bytes;
    char				*pos;
};

size_t asn1_read_length(char *pos, unsigned int *length, int left);
int asn1_read_tlv(char *buffer, int size, struct asn1_tlv_s *tlv);

#endif
