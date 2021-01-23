sbin_PROGRAMS = osns

extra_CFLAGS = -fasynchronous-unwind-tables -fexceptions -fstack-clash-protection

osns_CFLAGS = $(CFLAGS) $(GLIB_CFLAGS) $(LIBGCRYPT_CFLAGS) $(SYSTEMD_CFLAGS) $(AVAHI_CORE_CFLAGS) $(AVAHI_CLIENT_CFLAGS) -ggdb -Og  -D_FILE_OFFSET_BITS=64 -lz -lpthread -fPIC -lm -I . -I ./ssh -I ./lib -Wall -Wno-unused-but-set-variable -Wno-uninitialized -Wno-unused-variable -Wno-unused-label -Wno-unused-function
osns_CPPFLAGS = $(CXXFLAGS) $(GLIB_CFLAGS) $(LIBGCRYPT_CFLAGS) $(SYSTEMD_CFLAGS)  $(AVAHI_CORE_CFLAGS) $(AVAHI_CLIENT_CFLAGS) -ggdb -Og -D_FILE_OFFSET_BITS=64 -lz -lpthread -fPIC -lm -I . -I ./ssh -I ./lib -Wall -Wno-unused-but-set-variable -Wno-uninitialized -Wno-unused-variable -Wno-unused-label -Wno-unused-function
osns_LDADD = $(GLIB_LIBS) $(LIBGCRYPT_LIBS) $(SYSTEMD_LIBS) $(AVAHI_CORE_LIBS) $(AVAHI_CLIENT_LIBS) $(LDFLAGS)

osns_SOURCES =  	lib/fuse/directory.c lib/fuse/dentry.c lib/fuse/utils.c \
			lib/fuse/fuse-interface.c lib/fuse/fs.c \
			lib/fuse/fs-common.c lib/fuse/fs-virtual.c lib/fuse/fs-special.c \
			lib/fuse/context-fs-common.c lib/fuse/context-fs-root.c lib/fuse/context-fs.c \
			lib/sl/skiplist.c lib/sl/skiplist-delete.c lib/sl/skiplist-find.c lib/sl/skiplist-insert.c lib/sl/skiplist-utils.c lib/sl/skiplist-lock.c \
			lib/threads/workerthreads.c \
			lib/discover/discover.c lib/discover/discover-staticfile.c lib/discover/discover-avahi.c \
			lib/error/error-handling.c \
			lib/network/connection.c lib/network/utils.c \
			lib/eventloop/beventloop.c lib/eventloop/beventsignal.c lib/eventloop/beventtimer.c lib/eventloop/bevent.c \
			lib/workspace/workspaces.c lib/workspace/utils.c lib/workspace/session.c lib/workspace/context.c \
			lib/workspace/path-caching.c \
			lib/workspace/address.c \
			lib/interface/workspace.c \
			lib/misc/pidfile.c \
			lib/misc/utils.c lib/misc/pathinfo.c \
			lib/list/simple-hash.c lib/list/simple-list.c lib/list/simple-locking.c \
			lib/mountinfo/monitor.c lib/mountinfo/utils.c lib/mountinfo/mountinfo.c lib/mountinfo/monitormounts.c \
			lib/fsnotify/fschangenotify.c \
			lib/log/logging.c \
			lib/mime/handlemime.c  lib/misc/replaceandskip.c \
			options.c \
			main.c \
			lib/datatypes/ssh-string.c lib/datatypes/ssh-uint.c \
			lib/datatypes/ssh-namelist.c lib/datatypes/ssh-payload.c lib/datatypes/ssh-mpint.c lib/datatypes/ssh-msg-buffer.c \
			lib/users/check.c lib/users/local.c lib/users/monitor.c \
			fuse/sftp.c fuse/sftp-fs.c fuse/network.c fuse/ssh.c \
			fuse/sftp/inode-stat.c \
			fuse/sftp/getattr.c fuse/sftp/lock.c fuse/sftp/lookup.c fuse/sftp/mk.c fuse/sftp/open.c fuse/sftp/opendir.c \
			fuse/sftp/realpath.c fuse/sftp/rm.c fuse/sftp/setattr.c fuse/sftp/statfs.c  fuse/sftp/symlink.c fuse/sftp/xattr.c \
			interface/fuse.c \
			interface/sftp-attr.c interface/sftp-extensions.c interface/sftp-prefix.c interface/sftp-send.c interface/sftp-wait-response.c interface/sftp.c \
			interface/ssh-utils.c interface/ssh.c \
			sftp/attr/buffer.c \
			sftp/attr/read-attr-v03.c sftp/attr/read-attr-v04.c sftp/attr/read-attr-v05.c sftp/attr/read-attr-v06.c \
			sftp/attr/write-attr-v03.c sftp/attr/write-attr-v04.c sftp/attr/write-attr-v05.c sftp/attr/write-attr-v06.c \
			sftp/attr/read-name-v03.c \
			sftp/recv/recv-v03.c sftp/recv/recv-v04.c sftp/recv/recv-v05.c sftp/recv/recv-v06.c \
			sftp/send/send-v03.c sftp/send/send-v04.c sftp/send/send-v05.c sftp/send/send-v06.c \
			sftp/user/mapping.c \
			sftp/attr.c sftp/context.c sftp/common.c sftp/extensions.c sftp/init.c   \
			sftp/request-hash.c sftp/recv.c sftp/time.c sftp/usermapping.c \
			ssh/channel/init.c ssh/channel/payload.c ssh/channel/shell.c ssh/channel/startclose.c \
			ssh/channel/table.c ssh/channel/uri.c ssh/channel/utils.c \
			ssh/connections/connect.c  ssh/connections/init.c ssh/connections/setup.c ssh/connections/utils.c \
			ssh/extensions/extension.c \
			ssh/hash/hash.c \
			ssh/keyexchange/algo-exchange.c ssh/keyexchange/compare.c ssh/keyexchange/createkeys.c ssh/keyexchange/dh.c ssh/keyexchange/ecdh.c ssh/keyexchange/key-exchange.c ssh/keyexchange/keyx.c \
			ssh/pk/asn1.c ssh/pk/openssh-localdb.c ssh/pk/openssh-utils.c ssh/pk/openssh-cert.c ssh/pk/pk-create.c \
			ssh/pk/pk-types.c ssh/pk/pk-keys.c ssh/pk/pk-compare.c ssh/pk/pk-keystore.c ssh/pk/pk-keystore-openssh.c \
			ssh/pk/pk-layout.c ssh/pk/pk-read-private.c ssh/pk/pk-readwrite-public.c ssh/pk/pk-sign.c ssh/pk/pk-utils.c ssh/pk/pk-verify.c \
			ssh/receive/decompress.c ssh/receive/decompress-none.c ssh/receive/decompressors.c ssh/receive/decrypt.c ssh/receive/decrypt-chacha20-poly1305.c \
			ssh/receive/decrypt-generic.c ssh/receive/decryptors.c ssh/receive/greeter.c ssh/receive/init.c ssh/receive/msg-channel.c ssh/receive/msg-transport.c ssh/receive/msg-userauth.c \
			ssh/receive/payload.c ssh/receive/read-buffer.c ssh/receive/read-socket.c \
			ssh/send/compress.c ssh/send/compress-none.c ssh/send/compressors.c ssh/send/encrypt.c ssh/send/encrypt-chacha20-poly1305.c ssh/send/encrypt-generic.c \
			ssh/send/encryptors.c ssh/send/greeter.c ssh/send/init.c ssh/send/msg-channel.c ssh/send/msg-transport.c ssh/send/msg-userauth.c ssh/send/send.c ssh/send/write-socket.c \
			ssh/ssh-userauth.c ssh/userauth/pubkey.c ssh/userauth/utils.c ssh/userauth/hostbased.c ssh/userauth/none.c ssh/userauth/password.c \
			ssh/ssh-common-protocol.c ssh/ssh-common.c \
			ssh/ssh-signal.c \
			ssh/ssh-data.c \
			ssh/ssh-hostinfo.c \
			ssh/ssh-language.c \
			ssh/ssh-pubkey.c \
			ssh/ssh-utils.c



if LIBRSYNC_AVAIL
osns_CFLAGS += -lrsync
osns_CPPFLAGS += -lrsync
osns_LDADD += -L/usr/lib64 -lrsync
endif

if GLIB2_AVAIL
osns_CFLAGS += $(GLIB_CFLAGS)
osns_CPPFLAGS += $(GLIB_CFLAGS)
osns_LDADD += $(GLIB_LIBS)
endif

if GOBJECT2_AVAIL
osns_CFLAGS += $(GOBJECT2_CFLAGS)
osns_CPPFLAGS += $(GOBJECT2_CFLAGS)
osns_LDADD += $(GOBJECT2_LIBS)
endif

if GIO2_AVAIL
osns_CFLAGS += $(GIO_CFLAGS)
osns_CPPFLAGS += $(GIO_CFLAGS)
osns_LDADD += $(GIO_LIBS)
endif

if LIBPAM_AVAIL
osns_CFLAGS += -lpam
osns_CPPFLAGS += -lpam
else
osns_CFLAGS += -lcrypt
osns_CPPFLAGS += -lcrypt
endif


