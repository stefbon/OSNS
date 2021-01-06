/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018 Stef Bon <stefbon@gmail.com>

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

#ifndef FS_WORKSPACE_SSH_PK_TYPES_H
#define FS_WORKSPACE_SSH_PK_TYPES_H

#define SSH_PKALGO_SCHEME_RSA				1
#define SSH_PKALGO_SCHEME_DSS				2
#define SSH_PKALGO_SCHEME_ECC				3

#define SSH_PKALGO_ID_DSS				1
#define SSH_PKALGO_ID_RSA				2
#define SSH_PKALGO_ID_ED25519				3
#define SSH_PKALGO_ID_RSA_SHA2_256			4
#define SSH_PKALGO_ID_RSA_SHA2_512			5
#define SSH_PKALGO_ID_RSA_CERT_V01_OPENSSH_COM		6
#define SSH_PKALGO_ID_DSS_CERT_V01_OPENSSH_COM		7
#define SSH_PKALGO_ID_ED25519_CERT_V01_OPENSSH_COM	8

#define SSH_PKALGO_FLAG_PKA				1				/* Public Key Algorithm */
#define SSH_PKALGO_FLAG_PKC				2				/* Public Key Certificate */
#define SSH_PKALGO_FLAG_DEFAULT				4
#define SSH_PKALGO_FLAG_PREFERRED			8
#define SSH_PKALGO_FLAG_OPENSSH_COM_CERTIFICATE		16

/* rsa options */

#define SSH_PKALGO_OPTION_RSA_BITS_UNKNOWN		1
#define SSH_PKALGO_OPTION_RSA_BITS_1024			2
#define SSH_PKALGO_OPTION_RSA_BITS_2048			4

/* dss options */

#define SSH_PKALGO_OPTION_DSS_BITS_UNKNOWN		1
#define SSH_PKALGO_OPTION_DSS_BITS_1024			2
#define SSH_PKALGO_OPTION_DSS_BITS_2048			4

/* ecc flags
    ecc does not use bits */

struct ssh_pkalgo_s {
    unsigned int			flags;
    unsigned int			scheme;
    unsigned int			id;
    const char				*name;
    const char				*libname;
    const char				*sshname;
    const char				*hash;
};

struct ssh_pkoptions_s {
    unsigned int			options;
};

void copy_pkalgo(struct ssh_pkalgo_s *a, struct ssh_pkalgo_s *b);
void set_pkoptions(struct ssh_pkoptions_s *options, struct ssh_pkalgo_s *pkalgo, unsigned int o);

struct ssh_pkalgo_s *get_pkalgo(char *algo, unsigned int len, int *index);
struct ssh_pkalgo_s *get_pkalgo_string(struct ssh_string_s *s, int *index);
struct ssh_pkalgo_s *get_pkalgo_byid(unsigned int id, int *index);

int get_index_pkalgo(struct ssh_pkalgo_s *algo);
struct ssh_pkalgo_s *get_next_pkalgo(struct ssh_pkalgo_s *algo, int *index);

unsigned int write_pkalgo(char *buffer, struct ssh_pkalgo_s *pkalgo);
void msg_write_pkalgo(struct msg_buffer_s *mb, struct ssh_pkalgo_s *pkalgo);
struct ssh_pkalgo_s *read_pkalgo(char *buffer, unsigned int size, int *read);
struct ssh_pkalgo_s *read_pkalgo_string(struct ssh_string_s *algo, int *read);

struct ssh_pkalgo_s *msg_read_pksignature(struct msg_buffer_s *mb1, struct ssh_string_s *algo, struct ssh_string_s *signature);
void msg_write_pksignature(struct msg_buffer_s *mb, struct ssh_pkalgo_s *pkalgo, struct ssh_string_s *signature);

const char *get_hashname_sign(struct ssh_pkalgo_s *pkalgo);

#endif
