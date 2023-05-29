/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021 Stef Bon <stefbon@gmail.com>

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
#include "libosns-datatypes.h"

#include "fssocket.h"
#include "fssocket-stat.h"

#ifdef __linux__

#include <dirent.h>

static int get_unix_fd_fs_socket(struct fs_socket_s *s)
{
    return s->fd;
}

static void set_unix_fd_fs_socket(struct fs_socket_s *s, int fd)
{
    s->fd=fd;
}

int compare_fs_socket(struct fs_socket_s *s, struct fs_socket_s *t)
{
    return ((s->fd==t->fd) ? 0 : -1);
}

static int fs_socket_open(struct fs_socket_s *ref, struct fs_path_s *path, struct fs_socket_s *sock, unsigned int flags, struct fs_init_s *init)
{
    int fd=-1;
    unsigned int size=fs_path_get_length(path);
    char tmp[size+1];

    if (size==0) {

        logoutput_error("fs_socket_open: name/path is empty");
        return -1;

    }

    if (sock->type == FS_SOCKET_TYPE_FILE) {

        if (flags & O_DIRECTORY) {

            logoutput_error("fs_socket_open: O_DIRECTORY bit set in flags while socket is file");
            return -1;

        }

    } else if (sock->type == FS_SOCKET_TYPE_DIR) {
        struct fs_socket_directory_data_s *data=&sock->data.dir;

        flags |= O_DIRECTORY;

        data->buffer=NULL;
        data->size=0;
        data->pos=0;
        data->read=0;
        data->dirent=NULL;

    } else {

        logoutput_error("fs_socket_open: socket type %u is not set/supported", sock->type);
        return -1;

    }

    memset(tmp, 0, size+1);
    size=fs_path_copy(path, tmp, size);

    if (ref) {

        if (ref->type != FS_SOCKET_TYPE_DIR) {

            logoutput_error("fs_socket_open: reference fs socket not a directory");
            return -1;

        }

        if (init) {

            fd=openat(ref->fd, tmp, flags, init->mode);

        } else {

            fd=openat(ref->fd, tmp, flags);

        }

    } else {

        if (init) {

            fd=open(tmp, flags, init->mode);

        } else {

            fd=open(tmp, flags);

        }

    }

    if (fd==-1) {

	logoutput_warning("fs_socket_open: error %i open (%s)", errno, strerror(errno));

    } else {

	sock->fd=fd;

    }

    return fd;

}

static int fs_socket_fsync(struct fs_socket_s *s, unsigned int flags)
{
    return (flags & FS_FSYNC_FLAG_DATA) ? fdatasync(s->fd) : fsync(s->fd);
}

static int fs_socket_flush(struct fs_socket_s *socket, unsigned int flags)
{
    /* what to do here ?? */
    return 0;
}

static void fs_socket_close(struct fs_socket_s *sock)
{

    if (sock->fd>=0) {

        close(sock->fd);
        sock->fd=-1;

    }

}

static void fs_socket_clear(struct fs_socket_s *sock)
{

    fs_socket_close(sock);

    if (sock->type == FS_SOCKET_TYPE_DIR) {

        if (sock->data.dir.buffer) {

            free(sock->data.dir.buffer);
            sock->data.dir.buffer=NULL;

        }

    }

    memset(sock, 0, sizeof(struct fs_socket_s));

}

/* FILE */

static int fs_socket_pread(struct fs_socket_s *s, char *data, unsigned int size, off_t off)
{
    return pread(s->fd, data, size, off);
}

static int fs_socket_pwrite(struct fs_socket_s *s, char *data, unsigned int size, off_t off)
{
    return pwrite(s->fd, data, size, off);
}

static off_t fs_socket_lseek(struct fs_socket_s *s, off_t off, int whence)
{
    return lseek(s->fd, off, whence);
}

/* DIRECTORY */

static void copy_dirent_2_dentry(struct linux_dirent64 *dirent, struct fs_dentry_s *dentry)
{

    dentry->flags=0;
    dentry->type=DTTOIF(dirent->d_type);
    dentry->ino=dirent->d_ino;
    dentry->len=(dirent->d_reclen - offsetof(struct linux_dirent, d_name) - 1);
    dentry->name=dirent->d_name;

}

static int get_dentry(struct fs_socket_s *s, struct fs_dentry_s *dentry)
{

    if (s->data.dir.dirent) {

        copy_dirent_2_dentry(s->data.dir.dirent, dentry);
        return 1;

    }

    return 0;

}

static int read_dentry(struct fs_socket_s *s, struct fs_dentry_s *dentry)
{
    struct fs_socket_directory_data_s *data=&s->data.dir;

    if (data->buffer==NULL) {

        data->buffer=malloc(FS_DIRECTORY_BUFFER_SIZE);

        if (data->buffer==NULL) {

            logoutput_error("read_dentry: unable to allocate %u bytes for directory buffer", FS_DIRECTORY_BUFFER_SIZE);
            return -1;

        }

        memset(data->buffer, 0, FS_DIRECTORY_BUFFER_SIZE);
        data->size=FS_DIRECTORY_BUFFER_SIZE;
        data->pos=0;
        data->read=0;

    }

    if (data->pos >= data->read) {
        int result=syscall(SYS_getdents64, s->fd, (struct linux_dirent64 *) data->buffer, data->size);

        if (result==-1) {

            logoutput_debug("read_dentry: error %u calling SYS_getdents64 (%s)", errno, strerror(errno));
            return -1;

        } else if (result==0) {

            return 0;

        }

        data->read=(unsigned int) result;
        data->pos=0;

    }

    data->dirent=(struct linux_dirent64 *) (data->buffer + data->pos);
    data->pos += data->dirent->d_reclen;
    if (dentry) copy_dirent_2_dentry(s->data.dir.dirent, dentry);
    return 1;

}

static int fs_socket_unlinkat_shared(struct fs_socket_s *ref, struct fs_path_s *path, int flags)
{
    unsigned int size=fs_path_get_length(path);
    char name[size+1];

    if (size==0) {

        logoutput_error("fs_socket_unlinkat_shared: name/path is empty");
        return -1;

    }

    memset(name, 0, size+1);
    size=fs_path_copy(path, name, size);
    return unlinkat(ref->fd, name, flags);
}


static int fs_socket_unlinkat(struct fs_socket_s *ref, struct fs_path_s *path)
{
    return fs_socket_unlinkat_shared(ref, path, 0);
}

static int fs_socket_rmdirat(struct fs_socket_s *ref, struct fs_path_s *path)
{
    return fs_socket_unlinkat_shared(ref, path, AT_REMOVEDIR);
}

static int fs_socket_readlinkat(struct fs_socket_s *ref, struct fs_path_s *path, struct fs_path_s *target)
{
    int len=-1;
    unsigned int size=512;
    unsigned int tmp=fs_path_get_length(path);
    char name[tmp+1];

    if (tmp==0) {

        logoutput_error("fs_socket_unlinkat_shared: name/path is empty");
        return -1;

    }

    memset(name, 0, tmp+1);
    size=fs_path_copy(path, name, tmp);

    target->buffer=realloc(target->buffer, size);
    if (target->buffer==NULL) return -1;
    target->len=0;
    target->size=size;
    target->flags=FS_PATH_FLAG_BUFFER_ALLOC;

    while (size < 4096) {

	len=readlinkat(ref->fd, name, target->buffer, size);

	if (len==-1) {

	    return -errno;

	} else if (len<size) {

	    target->len=len;
	    target->buffer[len]='\0';
	    break;

	}

	size+=512;

    }

    return len;

}

int get_full_path_readlink(struct fs_socket_s *sock, struct fs_path_s *path, struct fs_path_s *result)
{
    int tmp=-1;
    char procpath[64]; /* more than enough */
    struct fs_path_s tmppath=FS_PATH_INIT;

    tmp=snprintf(procpath, 64, "/proc/%i/fd/%i", getpid(), sock->fd);
    fs_path_assign_buffer(&tmppath, procpath, tmp);

    if (fs_path_get_target_unix_symlink(&tmppath, result)==0) {

        fs_path_append(result, 'p', (void *) path);
        tmp=(int) fs_path_get_length(result);

    }

    return tmp;

}

#else 

static int get_unix_fd_fs_socket(struct fs_socket_s *s)
{
    return -1;
}

static void set_unix_fd_fs_socket(struct fs_socket_s *s, int fd)
{
}

int compare_fs_socket(struct fs_socket_s *s, struct fs_socket_s *t)
{
    return -1;
}

static int fs_socket_open(struct fs_socket_s *ref, struct fs_location_path_s *path, struct fs_socket_s *sock, unsigned int flags, struct fs_init_s *init)
{
    return -1;
}

static int fs_socket_pread(struct fs_socket_s *s, char *data, unsigned int size, off_t off)
{
    return -1;
}

static int fs_socket_pwrite(struct fs_socket_s *s, char *data, unsigned int size, off_t off)
{
    return -1;
}

static int fs_socket_fsync(struct fs_socket_s *s)
{
    return -1;
}

static int fs_socket_fdatasync(struct fs_socket_s *s)
{
    return -1;
}

static int fs_socket_flush(struct fs_socket_s *socket, unsigned int flags)
{
    return -1;
}

static off_t fs_socket_lseek(struct fs_socket_s *s, off_t off, int whence)
{
    return (off_t) -1;
}

static void fs_socket_close(struct fs_socket_s *s)
{
}

static void fs_socket_clear(struct fs_socket_s *s)
{
}

static int get_dentry(struct fs_socket_s *s, struct fs_dentry_s *dentry)
{
    return -1;
}

static int read_dentry(struct fs_socket_s *s, struct fs_dentry_s *dentry)
{
    return -1;
}

static int fs_socket_unlinkat(struct fs_socket_s *ref, struct fs_path_s *path)
{
    return -1;
}

static int fs_socket_rmdirat(struct fs_socket_s *ref, struct fs_path_s *path)
{
    return -1;
}

static int fs_socket_readlinkat(struct fs_socket_s *ref, struct fs_path_s *path, struct fs_path_s *target)
{
    return -1;
}

#endif

void init_fs_socket(struct fs_socket_s *s, unsigned int type)
{

    memset(s, 0, sizeof(struct fs_socket_s));

#ifdef __linux__
    set_unix_fd_fs_socket(s, -1);
#endif

    s->close=fs_socket_close;
    s->clear=fs_socket_clear;
    s->get_unix_fd=get_unix_fd_fs_socket;
    s->set_unix_fd=set_unix_fd_fs_socket;

    s->ops.open=fs_socket_open;
    s->ops.fsync=fs_socket_fsync;
    s->ops.flush=fs_socket_flush;
    s->ops.fgetstat=fs_socket_fgetstat;
    s->ops.fsetstat=fs_socket_fsetstat;

    if (type==FS_SOCKET_TYPE_FILE) {

        s->type=FS_SOCKET_TYPE_FILE;

        s->ops.type.file.pread=fs_socket_pread;
        s->ops.type.file.pwrite=fs_socket_pwrite;
        s->ops.type.file.lseek=fs_socket_lseek;

    } else if (type==FS_SOCKET_TYPE_DIR) {

        s->type=FS_SOCKET_TYPE_DIR;

        s->ops.type.dir.get_dentry=get_dentry;
        s->ops.type.dir.read_dentry=read_dentry;
        s->ops.type.dir.fstatat=fs_socket_fgetstatat;
        s->ops.type.dir.unlinkat=fs_socket_unlinkat;
        s->ops.type.dir.rmdirat=fs_socket_rmdirat;
        s->ops.type.dir.readlinkat=fs_socket_readlinkat;

    }

}
