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
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "log.h"

#include "ssh-mpint.h"
#include "ssh-utils.h"

#if HAVE_LIBGCRYPT
#include <gcrypt.h>
#endif

static int testnumber=9;

struct ssh_mpint_ops_s {
    void 		(* init)(struct ssh_mpint_s *mp);
    int			(* create)(struct ssh_mpint_s *mp);
    void 		(* free)(struct ssh_mpint_s *mp);
    unsigned int 	(* get_nbits)(struct ssh_mpint_s *mp);
    unsigned int 	(* get_nbytes)(struct ssh_mpint_s *mp);
    void 		(* power_modulo)(struct ssh_mpint_s *r, struct ssh_mpint_s *b, struct ssh_mpint_s *e, struct ssh_mpint_s *m);
    int			(* compare)(struct ssh_mpint_s *a, struct ssh_mpint_s *b);
    int 		(* compare_ui)(struct ssh_mpint_s *a, unsigned long l);
    void 		(* swap)(struct ssh_mpint_s *a, struct ssh_mpint_s *b);
    int 		(* invm)(struct ssh_mpint_s *x, struct ssh_mpint_s *a, struct ssh_mpint_s *m);
    int 		(* randomize)(struct ssh_mpint_s *mp, unsigned int bits);
    void		(* add)(struct ssh_mpint_s *w, struct ssh_mpint_s *u, const char *what, unsigned long n);
    void		(* mul)(struct ssh_mpint_s *result, struct ssh_mpint_s *u, const char *what, unsigned long n);
    int			(* read)(struct ssh_mpint_s *mp, char *buffer, unsigned int size, unsigned int format, unsigned int *error);
    int			(* write)(struct ssh_mpint_s *mp, char *buffer, unsigned int size, unsigned int format, unsigned int *error);
    void		(* msg_read)(struct msg_buffer_s *mb, struct ssh_mpint_s *mp, unsigned int *plen);
    void		(* msg_write)(struct msg_buffer_s *mb, struct ssh_mpint_s *mp);
};

struct ssh_mpoint_ops_s {
    void 		(* free)(struct ssh_mpoint_s *mp);
    void 		(* init)(struct ssh_mpoint_s *mp);
    int			(* compare)(struct ssh_mpoint_s *a, struct ssh_mpoint_s *b);
    int			(* read)(struct ssh_mpoint_s *mp, char *buffer, unsigned int size, unsigned int format, unsigned int *error);
    void 		(* msg_read)(struct msg_buffer_s *mb, struct ssh_mpoint_s *mp, unsigned int *plen);
    int 		(* write)(struct ssh_mpoint_s *mp, char *buffer, unsigned int size, unsigned int format, unsigned int *error);
    void 		(* msg_write)(struct msg_buffer_s *mb, struct ssh_mpoint_s *mp);
};

#if HAVE_LIBGCRYPT

struct ssh_mpint_s zeroinit = {
    .type=SSH_MPINT_TYPE_GCRYPT,
    .ptr=NULL,
};

static void _init_ssh_mpint(struct ssh_mpint_s *mp)
{
    mp->type=SSH_MPINT_TYPE_GCRYPT;
    mp->ptr=NULL;
}

static int _create_ssh_mpint(struct ssh_mpint_s *mp)
{
    mp->ptr=(void *) gcry_mpi_new(0);
    return (mp->ptr) ? 0 : -1;
}

static void _free_ssh_mpint(struct ssh_mpint_s *mp)
{
    if (mp->ptr) {

	gcry_mpi_release((gcry_mpi_t) mp->ptr);
	mp->ptr=NULL;

    }
}

static unsigned int _get_nbits_ssh_mpint(struct ssh_mpint_s *mp)
{

    if (mp->ptr) {
	gcry_mpi_t mpi=(gcry_mpi_t) mp->ptr;

	unsigned int nbits=gcry_mpi_get_nbits(mpi);
	// logoutput("get_nbits_ssh_mpint: %i", nbits);
	return nbits;

    }

    return 0;
}

static unsigned int _get_nbytes_ssh_mpint(struct ssh_mpint_s *mp)
{
    unsigned int bytes = 0;

    if (mp->ptr) {
	gcry_mpi_t mpi=(gcry_mpi_t) mp->ptr;
	unsigned int bits = gcry_mpi_get_nbits(mpi);

	bytes = (bits / 8);

	if ((bits % 8) == 0) {

	    /* test highest bit is set add an extry byte to prevent it's read as negative */
	    if (gcry_mpi_test_bit(mpi, bits)) bytes++;

	} else {

	    /* bits does not fit in bytes */
	    bytes++;

	}

    }

    // logoutput("get_nbytes_ssh_mpint: %i", bytes);
    return bytes;
}

static void _power_modulo_ssh_mpint(struct ssh_mpint_s *result, struct ssh_mpint_s *base, struct ssh_mpint_s *exponent, struct ssh_mpint_s *modulo)
{

    if (result->ptr && base->ptr && exponent->ptr && modulo->ptr) {
	gcry_mpi_t r=(gcry_mpi_t) result->ptr;
	gcry_mpi_t b=(gcry_mpi_t) base->ptr;
	gcry_mpi_t e=(gcry_mpi_t) exponent->ptr;
	gcry_mpi_t m=(gcry_mpi_t) modulo->ptr;

	logoutput("power_modulo_ssh_mpint: r %s %i", (r)?"def":"nul", get_nbits_ssh_mpint(result));
	logoutput("power_modulo_ssh_mpint: b %s %i", (b)?"def":"nul", get_nbits_ssh_mpint(base));
	logoutput("power_modulo_ssh_mpint: e %s %i", (e)?"def":"nul", get_nbits_ssh_mpint(exponent));
	logoutput("power_modulo_ssh_mpint: m %s %i", (m)?"def":"nul", get_nbits_ssh_mpint(modulo));

	gcry_mpi_powm(r, b, e, m);

    }
}

static int _compare_ssh_mpint(struct ssh_mpint_s *a, struct ssh_mpint_s *b)
{
    if (a->ptr==NULL || b->ptr==NULL) {

	logoutput("compare_ssh_mpint: one or both arguments not defined (first %s second %s)", (a->ptr) ? "defined" : "notdefined", (b->ptr) ? "defined" : "notdefined");
	return -1;

    }
    return gcry_mpi_cmp((gcry_mpi_t) a->ptr, (gcry_mpi_t) b->ptr);
}

static int _compare_ssh_mpint_ui(struct ssh_mpint_s *a, unsigned long l)
{
    return gcry_mpi_cmp_ui((gcry_mpi_t) a->ptr, l);
}

static void _swap_ssh_mpint(struct ssh_mpint_s *a, struct ssh_mpint_s *b)
{
    gcry_mpi_swap((gcry_mpi_t) a->ptr, (gcry_mpi_t) b->ptr);
}

static int _invm_ssh_mpint(struct ssh_mpint_s *x, struct ssh_mpint_s *a, struct ssh_mpint_s *m)
{
    return gcry_mpi_invm((gcry_mpi_t) x->ptr, (gcry_mpi_t) a->ptr, (gcry_mpi_t) m->ptr);
}

static int _randomize_ssh_mpint(struct ssh_mpint_s *mp, unsigned int bits)
{
    // logoutput("_randomize_ssh_mpint: pre %i", get_nbits_ssh_mpint(mp));
    gcry_mpi_randomize((gcry_mpi_t) mp->ptr, bits, GCRY_WEAK_RANDOM);
    // logoutput("_randomize_ssh_mpint: post %i", get_nbits_ssh_mpint(mp));
    return 0;
}

static void _add_ssh_mpint(struct ssh_mpint_s *w, struct ssh_mpint_s *u, const char *what, unsigned long n)
{
    if (strcmp(what, "add")==0) {

	gcry_mpi_add_ui((gcry_mpi_t) w->ptr, (gcry_mpi_t) u->ptr, n);

    } else if (strcmp(what, "sub")==0) {

	gcry_mpi_sub_ui((gcry_mpi_t) w->ptr, (gcry_mpi_t) u->ptr, n);

    }
}

static void _mul_ssh_mpint(struct ssh_mpint_s *result, struct ssh_mpint_s *u, const char *what, unsigned long n)
{

    if (strcmp(what, "mul")==0) {

	gcry_mpi_mul_ui((gcry_mpi_t) result->ptr,(gcry_mpi_t) u->ptr, n);

    } else if (strcmp(what, "div")==0) {
	gcry_mpi_t tmp=gcry_mpi_set_ui(NULL, n);

	// logoutput("mul_ssh_mpint: div");
	gcry_mpi_div((gcry_mpi_t) result->ptr, NULL, (gcry_mpi_t) u->ptr, tmp, 0);

	gcry_mpi_release(tmp);

    }

}

static int _read_ssh_mpint(struct ssh_mpint_s *mp, char *buffer, unsigned int size, unsigned int format, unsigned int *error)
{
    size_t nscanned=0;
    gcry_error_t err=0;
    enum gcry_mpi_format mpi_format;
    gcry_mpi_t tmp=(gcry_mpi_t) mp->ptr;

    switch (format) {
    case SSH_MPINT_FORMAT_SSH :

	mpi_format=GCRYMPI_FMT_SSH;
	break;

    case SSH_MPINT_FORMAT_USC :

	mpi_format=GCRYMPI_FMT_USG;
	break;

    default :

	mpi_format=GCRYMPI_FMT_STD;

    }

    err=gcry_mpi_scan(&tmp, mpi_format, (const unsigned char *) buffer, (size_t) size, &nscanned);

    if (err) {

	logoutput("read_ssh_mpint: error %s/%s", gcry_strsource(err), gcry_strerror(err));

	*error=EIO;
	return -1;

    } else {

	logoutput_debug("read_ssh_mpint: %i bytes scanned", nscanned);
	mp->ptr=(void *) tmp;

    }

    return (int) nscanned;
}

static int _write_ssh_mpint(struct ssh_mpint_s *mp, char *buffer, unsigned int size, unsigned int format, unsigned int *error)
{
    size_t nwritten=0;
    gcry_error_t err=0;
    enum gcry_mpi_format mpi_format;

    switch (format) {
    case SSH_MPINT_FORMAT_SSH :

	mpi_format=GCRYMPI_FMT_SSH;
	break;

    case SSH_MPINT_FORMAT_USC :

	mpi_format=GCRYMPI_FMT_USG;
	break;

    default :

	mpi_format=GCRYMPI_FMT_STD;

    }

    if (buffer==NULL) return (4 + get_nbytes_ssh_mpint(mp));

    err=gcry_mpi_print(mpi_format, (unsigned char *) buffer, (size_t) size, &nwritten, (gcry_mpi_t) mp->ptr);

    if (err) {

	logoutput("write_ssh_mpint: error %s/%s", gcry_strsource(err), gcry_strerror(err));

	*error=EIO;
	return -1;

    }

    return (int) nwritten;

}

static void _msg_read_ssh_mpint(struct msg_buffer_s *mb, struct ssh_mpint_s *mp, unsigned int *plen)
{
    size_t nscanned=0;
    gcry_error_t err=0;
    unsigned int len=(mb->len - mb->pos);
    gcry_mpi_t tmp=(gcry_mpi_t) mp->ptr;

    if (plen) len=*plen;

    err=gcry_mpi_scan(&tmp, GCRYMPI_FMT_SSH, (const unsigned char *) &mb->data[mb->pos], (size_t) len, &nscanned);

    if (err) {

	logoutput("msg_read_ssh_mpint: error %s/%s", gcry_strsource(err), gcry_strerror(err));
	set_msg_buffer_fatal_error(mb, EIO);

    }

    mb->pos += nscanned;
    mp->ptr=(void *) tmp;
    if (plen) ((*plen) -= nscanned);
}

static void _msg_write_ssh_mpint(struct msg_buffer_s *mb, struct ssh_mpint_s *mp)
{
    unsigned int len = 4 + get_nbytes_ssh_mpint(mp);

    if (mb->data) {

	if (mb->pos + len <= mb->len) {
	    size_t nwritten=0;
	    gcry_error_t err=gcry_mpi_print(GCRYMPI_FMT_SSH, (unsigned char *) &mb->data[mb->pos], (size_t)(mb->len - mb->pos), &nwritten, (gcry_mpi_t) mp->ptr);

	    if (err) {

		logoutput("msg_write_ssh_mpint: error %s/%s", gcry_strsource(err), gcry_strerror(err));
		set_msg_buffer_fatal_error(mb, EIO);
		mb->pos += (nwritten==0) ? len : nwritten;

	    } else {

		mb->pos += nwritten;

	    }

	} else {

	    set_msg_buffer_fatal_error(mb, ENOBUFS);
	    mb->pos += len;

	}

    } else {

	mb->pos += len;

    }

}

static void _free_ssh_mpoint(struct ssh_mpoint_s *mp)
{

    if (mp->ptr) {

	// gcry_mpi_set_opaque(mp->lib.mpi, NULL, 0); /* will release the pointer */
	logoutput("free_ssh_mpoint: gcry_mpi_release");
	gcry_mpi_release((gcry_mpi_t) mp->ptr);
	mp->ptr=NULL;

    }
}

static void _init_ssh_mpoint(struct ssh_mpoint_s *mp)
{
    mp->ptr=NULL;
}

static int _compare_ssh_mpoint(struct ssh_mpoint_s *a, struct ssh_mpoint_s *b)
{
    unsigned int alen=0;
    void *aptr=NULL;
    unsigned int blen=0;
    void *bptr=NULL;

    if (a->ptr==NULL || b->ptr==NULL) {

	logoutput("compare_ssh_mpoint: a and/or b not defined");
	return -1;

    }

    aptr=gcry_mpi_get_opaque((gcry_mpi_t) a->ptr, &alen);
    bptr=gcry_mpi_get_opaque((gcry_mpi_t) b->ptr, &blen);

    if (aptr && bptr && (alen==blen)) {

	if (memcmp(aptr, bptr, (alen/8))==0) return 0;

    }

    return -1;
}

static int _read_ssh_mpoint(struct ssh_mpoint_s *mp, char *buffer, unsigned int size, unsigned int format, unsigned int *error)
{
    unsigned int len=0;
    char *pos=buffer;

    if (format != SSH_MPINT_FORMAT_SSH) {

	logoutput("read_ssh_mpoint: format %i not supported", format);
	*error=EINVAL;
	return -1;

    }

    if (size > 4) {

	len=get_uint32(pos);
	pos+=4;

	// logoutput("read_ssh_mpoint: size: %i len %i", size, len);

	if (4 + len <= size) {
	    char *data=(char *) malloc(len);

	    if (data) {

		memcpy(data, pos, len);

		mp->ptr=(void *) gcry_mpi_set_opaque(NULL, (void *) data, (8 * len));

		if (mp->ptr==NULL) {

		    free(data);
		    return -1;

		} else {

		    pos+=len;

		}

	    } else {

		*error=ENOMEM;
		return -1;

	    }

	} else {

	    *error=ENOBUFS;
	    return -1;

	}

    } else {

	*error=ENOBUFS;
	return -1;

    }

    return (int)(pos - buffer);

}

static void _msg_read_ssh_mpoint(struct msg_buffer_s *mb, struct ssh_mpoint_s *mp, unsigned int *plen)
{

    if (mb->len - mb->pos > 4) {
	unsigned int len=0;

	len=get_uint32(&mb->data[mb->pos]);

	if (plen) {

	    if (*plen < len + 4) {

		set_msg_buffer_fatal_error(mb, ENOBUFS);
		return;

	    }

	    (*plen) -= (len + 4);

	}

	mb->pos+=4;

	if (mb->pos + len <= mb->len) {
	    char *buffer=(char *) malloc(len);

	    if (buffer) {

		memcpy(buffer, &mb->data[mb->pos], len);
		mp->ptr=(void *)gcry_mpi_set_opaque(NULL, (void *) buffer, (8 * len));

	    } else {

		set_msg_buffer_fatal_error(mb, ENOMEM);

	    }

	} else {

	    set_msg_buffer_fatal_error(mb, ENOBUFS);

	}

	mb->pos+=len;

    }

}

static int _write_ssh_mpoint(struct ssh_mpoint_s *mp, char *buffer, unsigned int size, unsigned int format, unsigned int *error)
{
    unsigned int len=0;
    void *ptr=NULL;

    if (format != SSH_MPINT_FORMAT_SSH) {

	logoutput("read_ssh_mpoint: format %i not supported", format);
	*error=EINVAL;
	return -1;

    }

    *error=EIO;
    ptr=gcry_mpi_get_opaque((gcry_mpi_t) mp->ptr, &len);

    if (ptr) {
	struct ssh_string_s tmp;

	init_ssh_string(&tmp);
	tmp.ptr=(char *) ptr;
	tmp.len = len/8;

	if (size >= tmp.len + 4) {

	    *error=0;
	    return write_ssh_string(buffer, size, 's', (void *) &tmp);

	}

    }

    return -1;

}

static void _msg_write_ssh_mpoint(struct msg_buffer_s *mb, struct ssh_mpoint_s *mp)
{
    unsigned int len=0;
    void *ptr=NULL;

    ptr=gcry_mpi_get_opaque((gcry_mpi_t) mp->ptr, &len);

    if (ptr) {
	struct ssh_string_s tmp;

	init_ssh_string(&tmp);
	tmp.ptr=(char *) ptr;
	tmp.len = len/8;

	if ((mb->len - mb->pos) >= tmp.len + 4) {

	    msg_write_ssh_string(mb, 's', (void *) &tmp);
	    return;

	}

    }

    mb->error=EIO;

}

#else

struct ssh_mpint_s zeroinit = {
    .type=0,
    .ptr=NULL,
};

static int _create_ssh_mpint(struct ssh_mpint_s *mp)
{
    return -1;
}

static void _free_ssh_mpint(struct ssh_mpint_s *mp)
{
}

static void _init_ssh_mpint(struct ssh_mpint_s *mp)
{
}

static unsigned int _get_nbits_ssh_mpint(struct ssh_mpint_s *mp)
{
    return 0;
}

static unsigned int _get_nbytes_ssh_mpint(struct ssh_mpint_s *mp)
{
    return 0;
}

static void _power_modulo_ssh_mpint(struct ssh_mpint_s *result, struct ssh_mpint_s *b, struct ssh_mpint_s *e, struct ssh_mpint_s *m)
{
}

static int _compare_ssh_mpint(struct ssh_mpint_s *a, struct ssh_mpint_s *b)
{
    return -1;
}

static void _swap_ssh_mpint(struct ssh_mpint_s *a, struct ssh_mpint_s *b)
{
}

static int _invm_ssh_mpint(struct ssh_mpint_s *x, struct ssh_mpint_s *a, struct ssh_mpint_s *m)
{
    return -1;
}

static int _randomize_ssh_mpint(struct ssh_mpint_s *mp, unsigned int bits)
{
    return -1;
}

static int _read_ssh_mpint(struct ssh_mpint_s *mp, char *buffer, unsigned int size, unsigned int format, unsigned int *error)
{
    *error=EOPNOTSUPP;
    return -1;
}

static int _write_ssh_mpint(struct ssh_mpint_s *mp, char *buffer, unsigned int size, unsigned int format, unsigned int *error)
{
    *error=EOPNOTSUPP;
    return -1;
}

static void _msg_write_ssh_mpint(struct msg_buffer_s *mb, struct ssh_mpint_s *mp)
{
}

int _compare_ssh_mpoint(struct ssh_mpoint_s *a, struct ssh_mpoint_s *b)
{
    return -1;
}

static int _read_ssh_mpoint(struct ssh_mpoint_s *mp, char *buffer, unsigned int size, unsigned int format, unsigned int *error)
{
    return -1;
}

static void _msg_read_ssh_mpoint(struct msg_buffer_s *mb, struct ssh_mpoint_s *mp, unsigned int *plen)
{
}

static int _write_ssh_mpoint(struct ssh_mpoint_s *mp, char *buffer, unsigned int size, unsigned int format, unsigned int *error)
{
    return -1;
}

static void _msg_write_ssh_mpoint(struct msg_buffer_s *mb, struct ssh_mpoint_s *mp)
{
}

static void _free_ssh_mpoint(struct ssh_mpoint_s *mp)
{
}

static void _init_ssh_mpoint(struct ssh_mpoint_s *mp)
{
}

#endif

static struct ssh_mpint_ops_s ssh_mpint_ops = {
    .init				= _init_ssh_mpint,
    .create				= _create_ssh_mpint,
    .free				= _free_ssh_mpint,
    .get_nbits				= _get_nbits_ssh_mpint,
    .get_nbytes				= _get_nbytes_ssh_mpint,
    .power_modulo			= _power_modulo_ssh_mpint,
    .compare				= _compare_ssh_mpint,
    .compare_ui				= _compare_ssh_mpint_ui,
    .swap				= _swap_ssh_mpint,
    .invm				= _invm_ssh_mpint,
    .randomize				= _randomize_ssh_mpint,
    .add				= _add_ssh_mpint,
    .mul				= _mul_ssh_mpint,
    .read				= _read_ssh_mpint,
    .write				= _write_ssh_mpint,
    .msg_read				= _msg_read_ssh_mpint,
    .msg_write				= _msg_write_ssh_mpint,
};

int create_ssh_mpint(struct ssh_mpint_s *mp)
{
    return (* ssh_mpint_ops.create)(mp);
}

void free_ssh_mpint(struct ssh_mpint_s *mp)
{
    (* ssh_mpint_ops.free)(mp);
}

void init_ssh_mpint(struct ssh_mpint_s *mp)
{
    (* ssh_mpint_ops.init)(mp);
}

unsigned int get_nbits_ssh_mpint(struct ssh_mpint_s *mp)
{
    return (* ssh_mpint_ops.get_nbits)(mp);
}

unsigned int get_nbytes_ssh_mpint(struct ssh_mpint_s *mp)
{
    return (* ssh_mpint_ops.get_nbytes)(mp);
}

void power_modulo_ssh_mpint(struct ssh_mpint_s *result, struct ssh_mpint_s *b, struct ssh_mpint_s *e, struct ssh_mpint_s *m)
{
    (* ssh_mpint_ops.power_modulo)(result, b, e, m);
}

int compare_ssh_mpint(struct ssh_mpint_s *a, struct ssh_mpint_s *b)
{
    return (* ssh_mpint_ops.compare)(a, b);
}

int compare_ssh_mpint_ui(struct ssh_mpint_s *a, unsigned long l)
{
    return (* ssh_mpint_ops.compare_ui)(a, l);
}

void swap_ssh_mpint(struct ssh_mpint_s *a, struct ssh_mpint_s *b)
{
    (* ssh_mpint_ops.swap)(a, b);
}

int invm_ssh_mpint(struct ssh_mpint_s *x, struct ssh_mpint_s *a, struct ssh_mpint_s *m)
{
    return (* ssh_mpint_ops.invm)(x, a, m);
}

int randomize_ssh_mpint(struct ssh_mpint_s *mp, unsigned int bits)
{
    (* ssh_mpint_ops.randomize)(mp, bits);
    return (mp->ptr) ? 0 : -1;
}

void add_ssh_mpint(struct ssh_mpint_s *w, struct ssh_mpint_s *u, const char *what, unsigned long n)
{
    (* ssh_mpint_ops.add)(w, u, what, n);
}

void mul_ssh_mpint(struct ssh_mpint_s *w, struct ssh_mpint_s *u, const char *what, unsigned long n)
{
    (* ssh_mpint_ops.mul)(w, u, what, n);
}

int read_ssh_mpint(struct ssh_mpint_s *mp, char *buffer, unsigned int size, unsigned int format, unsigned int *error)
{
    return (* ssh_mpint_ops.read)(mp, buffer, size, format, error);
}

int write_ssh_mpint(struct ssh_mpint_s *mp, char *buffer, unsigned int size, unsigned int format, unsigned int *error)
{
    return (* ssh_mpint_ops.write)(mp, buffer, size, format, error);
}

void msg_read_ssh_mpint(struct msg_buffer_s *mb, struct ssh_mpint_s *mp, unsigned int *plen)
{
    (* ssh_mpint_ops.msg_read)(mb, mp, plen);
}

void msg_write_ssh_mpint(struct msg_buffer_s *mb, struct ssh_mpint_s *mp)
{
    (* ssh_mpint_ops.msg_write)(mb, mp);
}

static struct ssh_mpoint_ops_s ssh_mpoint_ops = {
    .free		= _free_ssh_mpoint,
    .init		= _init_ssh_mpoint,
    .compare		= _compare_ssh_mpoint,
    .read		= _read_ssh_mpoint,
    .msg_read 		= _msg_read_ssh_mpoint,
    .write 		= _write_ssh_mpoint,
    .msg_write 		= _msg_write_ssh_mpoint,
};

void free_ssh_mpoint(struct ssh_mpoint_s *mp)
{
    (* ssh_mpoint_ops.free)(mp);
}

void init_ssh_mpoint(struct ssh_mpoint_s *mp)
{
    (* ssh_mpoint_ops.init)(mp);
}

int compare_ssh_mpoint(struct ssh_mpoint_s *a, struct ssh_mpoint_s *b)
{
    return (* ssh_mpoint_ops.compare)(a, b);
}

int read_ssh_mpoint(struct ssh_mpoint_s *mp, char *buffer, unsigned int size, unsigned int format, unsigned int *error)
{
    return (* ssh_mpoint_ops.read)(mp, buffer, size, format, error);
}

void msg_read_ssh_mpoint(struct msg_buffer_s *mb, struct ssh_mpoint_s *mp, unsigned int *plen)
{
    (* ssh_mpoint_ops.msg_read)(mb, mp, plen);
}

int write_ssh_mpoint(struct ssh_mpoint_s *mp, char *buffer, unsigned int size, unsigned int format, unsigned int *error)
{
    return (* ssh_mpoint_ops.write)(mp, buffer, size, format, error);
}

void msg_write_ssh_mpoint(struct msg_buffer_s *mb, struct ssh_mpoint_s *mp)
{
    (* ssh_mpoint_ops.msg_write)(mb, mp);
}

