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

#include "libosns-basic-system-headers.h"

#include "libosns-log.h"
#include "ssh-uint.h"

void store_uint16(char *buff, uint16_t value)
{
    unsigned char two[2];

    two[0] = (value >> 8) & 0xFF;
    two[1] = value & 0xFF;
    memcpy(buff, (char *) two, 2);

}

void store_uint32(char *buff, uint32_t value)
{
    unsigned char four[4];

    four[0] = (value >> 24) & 0xFF;
    four[1] = (value >> 16) & 0xFF;
    four[2] = (value >> 8) & 0xFF;
    four[3] = value & 0xFF;
    memcpy(buff, (char *) four, 4);

    // logoutput_debug("store_uint32: array %.*s buffer %.*s value %u", 4, four, 4, buff, value);

}

void store_uint64(char *buff, uint64_t value)
{
    unsigned char eight[8];

    eight[0] = (value >> 56) & 0xFF;
    eight[1] = (value >> 48) & 0xFF;
    eight[2] = (value >> 40) & 0xFF;
    eight[3] = (value >> 32) & 0xFF;
    eight[4] = (value >> 24) & 0xFF;
    eight[5] = (value >> 16) & 0xFF;
    eight[6] = (value >> 8) & 0xFF;
    eight[7] = value & 0xFF;
    memcpy(buff, (char *) eight, 8);

}

uint32_t get_uint32(char *buf)
{
    unsigned char *tmp=(unsigned char *) buf;
    return (uint32_t) (((uint32_t) tmp[0] << 24) | ((uint32_t) tmp[1] << 16) | ((uint32_t) tmp[2] << 8) | (uint32_t) tmp[3]);
}

uint16_t get_uint16(char *buf)
{
    unsigned char *tmp=(unsigned char *) buf;
    return (uint16_t) ((tmp[0] << 8) | tmp[1]);
}

uint64_t get_uint64(char *buf)
{
    unsigned char *tmp=(unsigned char *) buf;
    uint64_t a;
    uint32_t b;

    a = (uint64_t) (((uint64_t) tmp[0] << 56) | ((uint64_t) tmp[1] << 48) | ((uint64_t) tmp[2] << 40) | ((uint64_t) tmp[3] << 32));
    b = (uint32_t) (((uint32_t) tmp[4] << 24) | ((uint32_t) tmp[5] << 16) | ((uint32_t) tmp[6] << 8) | (uint32_t) tmp[7]);

    return (uint64_t)(a | b);
}

int64_t get_int64(char *buf)
{
    unsigned char *tmp=(unsigned char *) buf;
    int64_t a;
    uint32_t b;

    a = (int64_t) (((int64_t) tmp[0] << 56) | ((int64_t) tmp[1] << 48) | ((int64_t) tmp[2] << 40) | ((int64_t) tmp[3] << 32));
    b = (int32_t) (((int32_t) tmp[4] << 24) | ((int32_t) tmp[5] << 16) | ((int32_t) tmp[6] << 8) | ((int32_t) tmp[7]));

    return (int64_t)(a | b);
}

