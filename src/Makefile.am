bin_PROGRAMS =
sbin_PROGRAMS =

# extra_CFLAGS = -fasynchronous-unwind-tables -fexceptions -fstack-clash-protection -arch x86_64 -Wall -Wextra -Wconversion -Wformat-security 
extra_CFLAGS = -fexceptions -fstack-clash-protection -arch x86_64 -Wall -Wextra -Wconversion -Wformat-security 
debug_CFLAGS = -ggdb -Og
# debug_CFLAGS =

osnssystem_CFLAGS = $(CFLAGS) $(GLIB_CFLAGS) $(debug_CFLAGS) -D_FILE_OFFSET_BITS=64 -lz -lpthread -fPIC -lm -I . -I include -I ./lib -Wall -Wno-unused-but-set-variable -Wno-uninitialized -Wno-unused-variable -Wno-unused-label -Wno-unused-function
osnssystem_CPPFLAGS = $(CXXFLAGS) $(GLIB_CFLAGS) $(debug_CFLAGS) -D_FILE_OFFSET_BITS=64 -lz -lpthread -fPIC -lm -I . -I ./ssh -I ./lib -Wall -Wno-unused-but-set-variable -Wno-uninitialized -Wno-unused-variable -Wno-unused-label -Wno-unused-function
osnssystem_LDADD = $(GLIB_LIBS)  $(LDFLAGS)

if BUILD_OSNS_SYSTEM
sbin_PROGRAMS += osnssystem
endif

osnssystem_SOURCES = 	osns_system.c system/dnssd.c system/dnssd-utils.c \
			system/mount.c  system/fuse.c \
			system/system.c system/config.c system/resources.c \
			osns/pidfile.c osns/control.c osns/connection.c \
			osns/recv/read.c osns/recv/msg-disconnect.c osns/recv/msg-init.c osns/recv/msg-mount.c osns/recv/msg-channel.c osns/recv/msg-watch.c \
			osns/recv/msg-query.c osns/recv/server.c \
			osns/send/hashtable.c osns/send/reply.c osns/send/send.c osns/send/write.c osns/send/channel.c \
			osns/utils.c osns/record.c osns/query.c osns/osns-channel-utils.c \
			osns/backup/query.c \
			lib/eventloop/beventloop.c lib/eventloop/bevent.c lib/eventloop/bevent-write.c lib/eventloop/beventsubsys.c \
			lib/datatypes/ssh-uint.c lib/datatypes/name-string.c lib/datatypes/buffer.c \
			lib/db/open.c lib/db/utils.c \
			lib/resources/check.c lib/resources/shared.c lib/resources/system.c \
			lib/list/simple-list.c lib/list/lock.c \
			lib/lock/signal.c lib/lock/lockflag.c lib/lock/setflag.c lib/lock/locking.c \
			lib/threads/workerthreads.c \
			lib/system/time.c lib/system/path.c lib/system/stat.c lib/system/open.c \
			lib/socket/socket.c lib/socket/common.c lib/socket/connection.c lib/socket/device.c lib/socket/utils.c lib/socket/event.c lib/socket/control.c \
			lib/system/location.c lib/system/fsrm.c \
			lib/log/logging.c \
			lib/error/error-handling.c \
			lib/network/address.c lib/network/dns.c lib/network/services.c lib/network/utils.c \
			lib/mountinfo/event.c lib/mountinfo/list.c lib/mountinfo/monitor.c lib/mountinfo/read.c lib/mountinfo/mountentry.c lib/mountinfo/utils.c \
			lib/connection/connection.c \
			lib/ssh/ssh-string.c \
			lib/fuse/mount.c lib/fuse/init.c lib/fuse/receive.c lib/fuse/utils.c  lib/fuse/utils-public.c \
			lib/misc/utils.c lib/misc/replaceandskip.c

# 			osns/netcache/query.c osns/netcache/read.c osns/netcache/write.c 

if WITH_LINUX_EPOLL

osnssystem_SOURCES += lib/eventloop/backend/linux-epoll.c

else

osnssystem_SOURCES += lib/eventloop/backend/glib-mainloop.c 

endif

if GIO2_AVAIL
osnssystem_CFLAGS += $(GIO_CFLAGS)
osnssystem_CPPFLAGS += $(GIO_CFLAGS)
osnssystem_LDADD += $(GIO_LIBS)
endif

if SQLITE_AVAIL
osnssystem_CFLAGS+=$(SQLITE3_CFLAGS) -I sqlite
osnssystem_CPPFLAGS+=$(SQLITE3_CFLAGS) -I sqlite
osnssystem_LDADD+=$(SQLITE3_LIBS)
endif

osnsctl_CFLAGS = $(CFLAGS) $(GLIB_CFLAGS) $(debug_CFLAGS) -D_FILE_OFFSET_BITS=64 -lz -lpthread -fPIC -lm -I . -I include -I lib -Wall -Wno-unused-but-set-variable -Wno-uninitialized -Wno-unused-variable -Wno-unused-label -Wno-unused-function
osnsctl_CPPFLAGS = $(CXXFLAGS) $(GLIB_CFLAGS) $(debug_CFLAGS) -D_FILE_OFFSET_BITS=64 -lz -lpthread -fPIC -lm -I . -I include -I lib -Wall -Wno-unused-but-set-variable -Wno-uninitialized -Wno-unused-variable -Wno-unused-label -Wno-unused-function
osnsctl_LDADD = $(GLIB_LIBS) $(LDFLAGS)

if GIO2_AVAIL
osnsctl_CFLAGS += $(GIO_CFLAGS)
osnsctl_CPPFLAGS += $(GIO_CFLAGS)
osnsctl_LDADD += $(GIO_LIBS)
endif

if BUILD_OSNSCTL
sbin_PROGRAMS += osnsctl
endif

osnsctl_SOURCES = 	osnsctl.c \
			ctl/send-channel.c ctl/options.c \
			osns/connection.c osns/control.c osns/pidfile.c osns/record.c osns/utils.c osns/osns-channel-utils.c \
			osns/recv/read.c osns/recv/msg-channel.c osns/recv/msg-disconnect.c osns/recv/msg-init.c \
			osns/send/channel.c osns/send/hashtable.c osns/send/init.c osns/send/reply.c osns/send/send.c osns/send/write.c \
			lib/eventloop/beventloop.c lib/eventloop/bevent.c lib/eventloop/bevent-write.c \
			lib/eventloop/backend/linux-epoll.c lib/eventloop/beventsubsys.c \
			lib/ssh/ssh-string.c \
			lib/datatypes/ssh-uint.c lib/datatypes/name-string.c lib/datatypes/buffer.c \
			lib/list/simple-list.c lib/list/lock.c lib/lock/signal.c lib/lock/lockflag.c lib/lock/setflag.c \
			lib/threads/workerthreads.c \
			lib/log/logging.c \
			lib/error/error-handling.c \
			lib/connection/connection.c \
			lib/system/time.c lib/system/stat.c \
			lib/socket/socket.c lib/socket/common.c lib/socket/connection.c lib/socket/device.c lib/socket/utils.c lib/socket/control.c \
			lib/socket/event.c \
			lib/network/utils.c lib/network/address.c lib/network/dns.c \
			lib/misc/utils.c lib/misc/replaceandskip.c lib/system/fsrm.c


osnsclient_CFLAGS = $(CFLAGS) $(GLIB_CFLAGS) $(LIBGCRYPT_CFLAGS) $(debug_CFLAGS) -D_FILE_OFFSET_BITS=64 -lz -lpthread -fPIC -lm -I . -I include -I ./ssh -I ./lib -Wall -Wno-unused-but-set-variable -Wno-uninitialized -Wno-unused-variable -Wno-unused-label -Wno-unused-function
osnsclient_CPPFLAGS = $(CXXFLAGS) $(GLIB_CFLAGS) $(LIBGCRYPT_CFLAGS) $(debug_CFLAGS) -D_FILE_OFFSET_BITS=64 -lz -lpthread -fPIC -lm -I . -I include -I ./ssh -I ./lib -Wall -Wno-unused-but-set-variable -Wno-uninitialized -Wno-unused-variable -Wno-unused-label -Wno-unused-function
osnsclient_LDADD = $(GLIB_LIBS) $(LIBGCRYPT_LIBS) $(LDFLAGS)

if BUILD_OSNS_CLIENT
sbin_PROGRAMS += osnsclient
endif

osnsclient_SOURCES =  	osns_client.c \
			lib/fuse/directory/create.c lib/fuse/directory/directory.c lib/fuse/directory/dops.c lib/fuse/directory/lock.c lib/fuse/directory/utils.c \
			lib/fuse/dentry/dentry.c lib/fuse/dentry/hashtable.c lib/fuse/dentry/inode.c lib/fuse/dentry/utils.c \
			lib/fuse/utils.c lib/fuse/receive.c lib/fuse/request.c \
			lib/fuse/fs-access.c lib/fuse/fs-common.c lib/fuse/fs-virtual.c lib/fuse/utils-public.c lib/fuse/symlink.c lib/fuse/utils-create.c \
			lib/fuse/fs-service-browse.c lib/fuse/fs-service-path.c lib/fuse/handle.c lib/fuse/opendir.c lib/fuse/openfile.c \
			lib/fuse/path.c lib/fuse/fs-create.c \
			fuse/disconnected-fs.c fuse/browse-fs.c fuse/sftp-fs.c \
			fuse/browse/getattr.c fuse/browse/lookup.c fuse/browse/opendir.c \
			fuse/sftp/fstatat.c fuse/sftp/getattr.c fuse/sftp/handle.c fuse/sftp/inode-stat.c fuse/sftp/lock.c fuse/sftp/lookup.c fuse/sftp/mk.c \
			fuse/sftp/open.c fuse/sftp/opendir.c  fuse/sftp/path.c fuse/sftp/realpath.c fuse/sftp/rm.c fuse/sftp/setattr.c fuse/sftp/statfs.c fuse/sftp/symlink.c \
			fuse/shared/access.c fuse/shared/xattr.c fuse/shared/statfs.c fuse/shared/name.c \
			client/arguments.c client/config.c client/workspaces.c client/network.c client/utils.c \
			client/refresh.c client/resources.c client/connect.c client/ssh.c client/sftp.c client/populate-network.c \
			client/channel/channel.c client/channel/ssh.c client/channel/utils.c \
			lib/db/open.c lib/db/utils.c \
			lib/resources/client.c lib/resources/shared.c \
			lib/sl/skiplist.c lib/sl/skiplist-delete.c lib/sl/skiplist-find.c lib/sl/skiplist-insert.c lib/sl/skiplist-utils.c lib/sl/skiplist-lock.c \
			lib/workspace/workspaces.c lib/workspace/utils.c lib/workspace/address.c \
			lib/context/context.c lib/context/init.c lib/context/next.c lib/context/inodes.c  \
			lib/interface/interface.c lib/interface/iocmd.c lib/interface/list.c \
			lib/misc/utils.c lib/misc/replaceandskip.c \
			osns/connection.c osns/control.c osns/record.c osns/utils.c osns/pidfile.c osns/query.c \
			osns/recv/read.c osns/recv/msg-channel.c osns/recv/msg-disconnect.c osns/recv/msg-query.c osns/recv/msg-init.c osns/recv/msg-mount.c \
			osns/send/channel.c osns/send/hashtable.c osns/send/init.c osns/send/mountcmd.c osns/send/reply.c osns/send/send.c osns/send/write.c \
			osns/osns-channel-utils.c \
			interface/fuse.c interface/ssh.c interface/ssh-utils.c interface/ssh-channel.c interface/osns-channel.c \
			lib/eventloop/beventloop.c lib/eventloop/bevent.c lib/eventloop/bevent-write.c \
			lib/eventloop/beventsignal.c lib/eventloop/beventsubsys.c \
			lib/eventloop/beventtimer.c lib/eventloop/backend/linux-timerfd.c lib/eventloop/backend/linux-signalfd.c \
			lib/datatypes/name.c lib/datatypes/buffer.c lib/datatypes/ssh-uint.c lib/datatypes/name-string.c \
			lib/datatypes/ssh-msg-buffer.c lib/datatypes/ssh-mpint.c lib/datatypes/ssh-namelist.c \
			lib/list/simple-hash.c lib/list/simple-list.c lib/list/lock.c \
			lib/lock/locking.c lib/lock/lockflag.c lib/lock/setflag.c lib/lock/signal.c \
			lib/threads/workerthreads.c \
			lib/error/error-handling.c \
			lib/log/logging.c \
			lib/system/fssocket.c lib/system/fssocket-stat.c \
			lib/fspath/append.c lib/fspath/combine.c lib/fspath/compare.c lib/fspath/copy.c lib/fspath/filename.c lib/fspath/misc.c lib/fspath/set.c \
			lib/system/stat.c lib/system/time.c lib/system/fsrm.c \
			lib/socket/socket.c lib/socket/common.c lib/socket/connection.c lib/socket/device.c lib/socket/event.c lib/socket/utils.c lib/socket/control.c \
			lib/connection/connection.c lib/connection/compare.c \
			lib/users/cache.c lib/users/check.c lib/users/domain.c lib/users/entity.c lib/users/getent.c lib/users/local.c lib/users/lookup.c \
			lib/users/mapcb.c lib/users/mapcb_p2l.c lib/users/mapcb_l2p.c lib/users/mapping.c \
			lib/network/dns.c lib/network/utils.c lib/network/address.c lib/network/services.c \
			lib/ssh/payload.c lib/ssh/ssh-string.c \
			ssh/channel/init.c ssh/channel/exec.c ssh/channel/startclose.c \
			ssh/channel/table.c ssh/channel/signal.c ssh/channel/uri.c ssh/channel/utils.c \
			ssh/connections/connect.c  ssh/connections/init.c ssh/connections/setup.c ssh/connections/utils.c \
			ssh/extensions/extension.c ssh/extensions/global-request.c \
			ssh/hash/hash.c \
			ssh/keyexchange/algo-exchange.c ssh/keyexchange/compare.c ssh/keyexchange/createkeys.c ssh/keyexchange/dh.c ssh/keyexchange/ecdh.c \
			ssh/keyexchange/key-exchange.c ssh/keyexchange/key-exchange-client.c ssh/keyexchange/keyx.c \
			ssh/pk/asn1.c ssh/pk/openssh-localdb.c ssh/pk/openssh-utils.c ssh/pk/openssh-cert.c ssh/pk/pk-create.c \
			ssh/pk/pk-types.c ssh/pk/pk-keys.c ssh/pk/pk-compare.c ssh/pk/pk-keystore.c ssh/pk/pk-keystore-openssh.c \
			ssh/pk/pk-layout.c ssh/pk/pk-read-private.c ssh/pk/pk-readwrite-public.c ssh/pk/pk-sign.c ssh/pk/pk-utils.c ssh/pk/pk-verify.c \
			ssh/receive/decompress.c ssh/receive/decompress-none.c ssh/receive/decompressors.c ssh/receive/decrypt.c ssh/receive/decrypt-chacha20-poly1305.c \
			ssh/receive/decrypt-generic.c ssh/receive/decryptors.c ssh/receive/greeter.c ssh/receive/init.c ssh/receive/msg-channel.c ssh/receive/msg-transport.c ssh/receive/msg-userauth.c \
			ssh/receive/payload.c ssh/receive/read-buffer.c ssh/receive/read-socket.c \
			ssh/send/compress.c ssh/send/compress-none.c ssh/send/compressors.c ssh/send/encrypt.c ssh/send/encrypt-chacha20-poly1305.c ssh/send/encrypt-generic.c \
			ssh/send/encryptors.c ssh/send/greeter.c ssh/send/init.c ssh/send/msg-channel.c ssh/send/msg-transport.c ssh/send/msg-userauth.c ssh/send/send.c ssh/send/write-socket.c \
			ssh/ssh-userauth.c ssh/userauth/pubkey.c ssh/userauth/utils.c ssh/userauth/hostbased.c ssh/userauth/none.c ssh/userauth/password.c \
			ssh/ssh-common-protocol.c ssh/ssh-common.c ssh/ssh-common-client.c ssh/ssh-signal-client.c ssh/ssh-common-crypto.c \
			ssh/ssh-data.c ssh/ssh-hostinfo.c ssh/ssh-language.c ssh/ssh-pubkey.c ssh/ssh-utils.c \
			ssh/users/mapping.c ssh/users/unknown.c \
			interface/sftp.c interface/sftp-attr.c interface/sftp-send.c interface/sftp-wait-response.c \
			lib/sftp/attr-context.c \
			lib/sftp/attr/init-attr-v03.c lib/sftp/attr/init-attr-v04.c lib/sftp/attr/init-attr-v05.c lib/sftp/attr/init-attr-v06.c \
			lib/sftp/attr/read-attr-v03.c lib/sftp/attr/read-attr-v04.c lib/sftp/attr/read-attr-v05.c lib/sftp/attr/read-attr-v06.c \
			lib/sftp/attr/write-attr-v03.c lib/sftp/attr/write-attr-v04.c lib/sftp/attr/write-attr-v05.c lib/sftp/attr/write-attr-v06.c \
			lib/sftp/rw-attr-generic.c \
			sftp/recv.c sftp/recv/recv-v03.c sftp/recv/recv-v04.c sftp/recv/recv-v05.c sftp/recv/recv-v06.c \
			sftp/send.c sftp/send/send-v03.c sftp/send/send-v04.c sftp/send/send-v05.c sftp/send/send-v06.c \
			sftp/attr.c sftp/context.c sftp/common.c sftp/extensions.c sftp/extensions/fsync.c sftp/extensions/statvfs.c \
			sftp/init.c   \
			sftp/request-hash.c sftp/time.c sftp/usermapping.c


if WITH_LINUX_EPOLL

osnsclient_SOURCES += lib/eventloop/backend/linux-epoll.c

else

osnsclient_SOURCES += lib/eventloop/backend/glib-mainloop.c 

endif


# if LIBRSYNC_AVAIL
# osns_client_CFLAGS += -lrsync
# osns_client_CPPFLAGS += -lrsync
# osns_client_LDADD += -L/usr/lib64 -lrsync
# endif

if GLIB2_AVAIL
osnsclient_CFLAGS += $(GLIB_CFLAGS)
osnsclient_CPPFLAGS += $(GLIB_CFLAGS)
osnsclient_LDADD += $(GLIB_LIBS)
endif

if GOBJECT2_AVAIL
osnsclient_CFLAGS += $(GOBJECT_CFLAGS)
osnsclient_CPPFLAGS += $(GOBJECT_CFLAGS)
osnsclient_LDADD += $(GOBJECT_LIBS)
endif

if GIO2_AVAIL
osnsclient_CFLAGS += $(GIO_CFLAGS)
osnsclient_CPPFLAGS += $(GIO_CFLAGS)
osnsclient_LDADD += $(GIO_LIBS)
endif

if LIBPAM_AVAIL
osnsclient_CFLAGS += -lpam
osnsclient_CPPFLAGS += -lpam
else
osnsclient_CFLAGS += -lcrypt
osnsclient_CPPFLAGS += -lcrypt
endif

# if LIBSYSTEMD_AVAIL
# osnsclient_CFLAGS += $(SYSTEMD_CFLAGS)
# osnsclient_CPPFLAGS += $(SYSTEMD_CFLAGS)
# osnsclient_LDADD += $(SYSTEMD_LIBS)
# endif

if SQLITE_AVAIL
osnsclient_CFLAGS+=$(SQLITE3_CFLAGS) -I sqlite
osnsclient_CPPFLAGS+=$(SQLITE3_CFLAGS) -I sqlite
osnsclient_LDADD+=$(SQLITE3_LIBS)
endif


# if LIBSMB2_AVAIL
# osns_client_CFLAGS += $(LIBSMB2_CFLAGS)
# osns_client_CPPFLAGS += $(LIBSMB2_CFLAGS)
# osns_client_LDADD += $(LIBSMB2_LIBS)
# endif

if BUILD_SSH_SERVER
sbin_PROGRAMS += osnsserver
endif

osnsserver_CFLAGS = $(CFLAGS) $(GLIB_CFLAGS) $(LIBGCRYPT_CFLAGS) $(SYSTEMD_CFLAGS) -ggdb -Og  -D_FILE_OFFSET_BITS=64 -lz -lpthread -fPIC -lm -I . -I ./ssh -I ./lib -Wall -Wno-unused-but-set-variable -Wno-uninitialized -Wno-unused-variable -Wno-unused-label -Wno-unused-function
osnsserver_CPPFLAGS = $(CXXFLAGS) $(GLIB_CFLAGS) $(LIBGCRYPT_CFLAGS) $(SYSTEMD_CFLAGS)  -ggdb -Og -D_FILE_OFFSET_BITS=64 -lz -lpthread -fPIC -lm -I . -I ./ssh -I ./lib -Wall -Wno-unused-but-set-variable -Wno-uninitialized -Wno-unused-variable -Wno-unused-label -Wno-unused-function
osnsserver_LDADD = $(GLIB_LIBS) $(LIBGCRYPT_LIBS) $(SYSTEMD_LIBS) $(LDFLAGS)

osnsserver_SOURCES =  	$(osns_common_SOURCES) \
			lib/workspace/workspaces.c lib/workspace/utils.c lib/workspace/session.c lib/workspace/context.c \
			lib/workspace/address.c lib/interface/workspace.c \
			lib/misc/pidfile.c lib/misc/utils.c lib/misc/pathinfo.c \
			lib/mountinfo/monitor.c lib/mountinfo/utils.c lib/mountinfo/mountinfo.c lib/mountinfo/monitormounts.c \
			lib/fsnotify/fschangenotify.c \
			lib/mime/handlemime.c lib/misc/replaceandskip.c \
			options.c \
			osns_server.c \
			lib/users/check.c lib/users/local.c \
			sftp/attr/buffer.c sftp/attr/read-attr-generic.c \
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
			ssh/extensions/extension.c ssh/extensions/global-request.c \
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
			ssh/ssh-common-protocol.c ssh/ssh-common.c ssh/ssh-signal.c ssh/ssh-data.c \
			ssh/ssh-hostinfo.c ssh/ssh-language.c ssh/ssh-pubkey.c ssh/ssh-utils.c

# if LIBRSYNC_AVAIL
# osns_server_CFLAGS += -lrsync
# osns_server_CPPFLAGS += -lrsync
# osns_server_LDADD += -L/usr/lib64 -lrsync
# endif

if GLIB2_AVAIL
osnsserver_CFLAGS += $(GLIB_CFLAGS)
osnsserver_CPPFLAGS += $(GLIB_CFLAGS)
osnsserver_LDADD += $(GLIB_LIBS)
endif

if GOBJECT2_AVAIL
osnsserver_CFLAGS += $(GOBJECT2_CFLAGS)
osnsserver_CPPFLAGS += $(GOBJECT2_CFLAGS)
osnsserver_LDADD += $(GOBJECT2_LIBS)
endif

if GIO2_AVAIL
osnsserver_CFLAGS += $(GIO_CFLAGS)
osnsserver_CPPFLAGS += $(GIO_CFLAGS)
osnsserver_LDADD += $(GIO_LIBS)
endif

if LIBPAM_AVAIL
osnsserver_CFLAGS += -lpam
osnsserver_CPPFLAGS += -lpam
else
osnsserver_CFLAGS += -lcrypt
osnsserver_CPPFLAGS += -lcrypt
endif

if BUILD_SFTP_SUBSYSTEM
bin_PROGRAMS += osns_sftp_subsystem
endif

osns_sftp_subsystem_CFLAGS = $(CFLAGS) $(GLIB_CFLAGS) -ggdb -Og  -D_FILE_OFFSET_BITS=64 -lz -lpthread -fPIC -lm -I . -I . -I lib -I include -Wall -Wno-unused-but-set-variable -Wno-uninitialized -Wno-unused-variable -Wno-unused-label -Wno-unused-function
osns_sftp_subsystem_CPPFLAGS = $(CXXFLAGS) $(GLIB_CFLAGS) -ggdb -Og -D_FILE_OFFSET_BITS=64 -lz -lpthread -fPIC -lm -I . -I lib -I include -Wall -Wno-unused-but-set-variable -Wno-uninitialized -Wno-unused-variable -Wno-unused-label -Wno-unused-function
osns_sftp_subsystem_LDADD = $(GLIB_LIBS) $(LDFLAGS)

osns_sftp_subsystem_SOURCES	=       osns_sftp_subsystem.c ssh/subsystem/connection.c ssh/subsystem/std.c \
					ssh/subsystem/sftp/attr.c \
					ssh/subsystem/sftp/cb-opendir.c ssh/subsystem/sftp/cb-stat.c \
					ssh/subsystem/sftp/cb-close.c ssh/subsystem/sftp/cb-open.c \
					ssh/subsystem/sftp/cb-readlink.c ssh/subsystem/sftp/cb-rm.c ssh/subsystem/sftp/cb-mk.c \
					ssh/subsystem/sftp/path.c \
					ssh/subsystem/sftp/handle.c ssh/subsystem/sftp/init.c \
					ssh/subsystem/sftp/receive.c ssh/subsystem/sftp/send.c \
					ssh/subsystem/sftp/supported.c ssh/subsystem/sftp/extensions.c \
					ssh/subsystem/sftp/extensions/setprefix.c ssh/subsystem/sftp/extensions/statvfs.c \
					ssh/subsystem/sftp/extensions/fsync.c ssh/subsystem/sftp/extensions/mapextension.c \
					ssh/subsystem/sftp/extensions/fstatat.c \
					lib/sftp/attr-context.c \
					lib/sftp/attr/init-attr-v03.c lib/sftp/attr/init-attr-v04.c lib/sftp/attr/init-attr-v05.c lib/sftp/attr/init-attr-v06.c \
					lib/sftp/attr/read-attr-v03.c lib/sftp/attr/read-attr-v04.c lib/sftp/attr/read-attr-v05.c lib/sftp/attr/read-attr-v06.c \
					lib/sftp/attr/write-attr-v03.c lib/sftp/attr/write-attr-v04.c lib/sftp/attr/write-attr-v05.c lib/sftp/attr/write-attr-v06.c \
					lib/sftp/rw-attr-generic.c \
					lib/users/getent.c \
					lib/users/local.c lib/users/lookup.c lib/users/mapcb.c \
					lib/users/mapcb_l2p.c lib/users/mapcb_p2l.c lib/users/mapping.c \
			                lib/system/fssocket.c lib/system/fssocket-stat.c \
			                lib/fspath/append.c lib/fspath/combine.c lib/fspath/compare.c lib/fspath/copy.c lib/fspath/filename.c lib/fspath/misc.c lib/fspath/set.c \
					osns/pidfile.c lib/misc/utils.c \
			                lib/eventloop/beventloop.c lib/eventloop/beventsignal.c lib/eventloop/bevent.c lib/eventloop/bevent-write.c lib/eventloop/beventsubsys.c \
			                lib/eventloop/backend/linux-timerfd.c lib/eventloop/backend/linux-signalfd.c \
			                lib/datatypes/ssh-uint.c lib/datatypes/name-string.c lib/datatypes/buffer.c \
			                lib/list/simple-list.c lib/list/lock.c \
			                lib/lock/signal.c lib/lock/lockflag.c lib/lock/setflag.c lib/lock/locking.c \
			                lib/threads/workerthreads.c \
			                lib/system/fshandle.c lib/system/time.c lib/system/stat.c \
			                lib/network/address.c \
			                lib/socket/socket.c lib/socket/common.c lib/socket/connection.c lib/socket/device.c lib/socket/utils.c lib/socket/event.c lib/socket/control.c \
			                lib/system/fsoperations.c \
			                lib/ssh/ssh-string.c \
			                lib/log/logging.c \
			                lib/error/error-handling.c


if WITH_LINUX_EPOLL

osns_sftp_subsystem_SOURCES += lib/eventloop/backend/linux-epoll.c

else

osns_sftp_subsystem_SOURCES += lib/eventloop/backend/glib-mainloop.c 

endif
