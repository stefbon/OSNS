bin_PROGRAMS =
sbin_PROGRAMS =

extra_CFLAGS = -fasynchronous-unwind-tables -fexceptions -fstack-clash-protection
# debug_CFLAGS = -ggdb -Og
debug_CFLAGS =


osns_common_SOURCES = 	lib/eventloop/beventloop.c lib/eventloop/bevent.c lib/eventloop/bevent-write.c \
			lib/eventloop/beventsignal.c lib/eventloop/beventsubsys.c lib/eventloop/beventtimer.c \
			lib/datatypes/name.c lib/datatypes/buffer.c \
			lib/datatypes/ssh-string.c lib/datatypes/ssh-uint.c \
			lib/datatypes/ssh-namelist.c lib/datatypes/ssh-payload.c lib/datatypes/ssh-msg-buffer.c \
			lib/list/simple-hash.c lib/list/simple-list.c lib/list/lock.c \
			lib/lock/locking.c \
			lib/threads/workerthreads.c \
			lib/error/error-handling.c \
			lib/log/logging.c \
			lib/system/directory.c lib/system/file.c lib/system/fshandle.c \
			lib/system/hash.c lib/system/insert.c lib/system/location.c \
			lib/system/open.c lib/system/path.c lib/system/stat.c lib/system/time.c \
			lib/socket/socket.c lib/socket/system.c lib/socket/common.c lib/socket/connection.c lib/socket/utils.c \
			lib/network/address.c

if LINUX

osns_common_SOURCES += lib/eventloop/backend/linux-signalfd.c lib/eventloop/backend/linux-timerfd.c

endif

if WITH_LINUX_EPOLL

osns_common_SOURCES += lib/eventloop/backend/linux-epoll.c

else

osns_common_SOURCES += lib/eventloop/backend/glib-mainloop.c 

endif

osns_system_CFLAGS = $(CFLAGS) $(GLIB_CFLAGS) $(AVAHI_CORE_CFLAGS) $(AVAHI_CLIENT_CFLAGS) $(AVAHI_GLIB_CFLAGS) $(debug_CFLAGS) -D_FILE_OFFSET_BITS=64 -lz -lpthread -fPIC -lm -I . -I include -I ./lib -Wall -Wno-unused-but-set-variable -Wno-uninitialized -Wno-unused-variable -Wno-unused-label -Wno-unused-function
osns_system_CPPFLAGS = $(CXXFLAGS) $(GLIB_CFLAGS) $(AVAHI_CORE_CFLAGS) $(AVAHI_CLIENT_CFLAGS) $(AVAHI_GLIB_CFLAGS) $(debug_CFLAGS) -D_FILE_OFFSET_BITS=64 -lz -lpthread -fPIC -lm -I . -I ./ssh -I ./lib -Wall -Wno-unused-but-set-variable -Wno-uninitialized -Wno-unused-variable -Wno-unused-label -Wno-unused-function
osns_system_LDADD = $(GLIB_LIBS) $(AVAHI_CORE_LIBS) $(AVAHI_CLIENT_LIBS) $(AVAHI_GLIB_LIBS) $(LDFLAGS)

if BUILD_OSNS_SYSTEM
sbin_PROGRAMS += osns_system
endif

osns_system_SOURCES = 	osns_system.c system/avahi-detect.c \
			system/mount.c  system/mountcmd.c system/fuse.c \
			system/netcache.c system/query-netcache.c system/query-netcache-filter.c \
			system/query-mountinfo.c \
			system/query.c system/query-filter.c \
			system/receive.c system/server.c \
			system/setwatch-netcache.c \
			osns/pidfile.c osns/control.c osns/receive.c osns/reply.c osns/write.c osns/utils.c \
			osns/netcache-read.c system/netcache-write.c \
			osns/record.c \
			lib/eventloop/beventloop.c lib/eventloop/bevent.c lib/eventloop/bevent-write.c \
			lib/eventloop/beventsubsys.c \
			lib/datatypes/ssh-string.c lib/datatypes/ssh-uint.c lib/datatypes/name-string.c lib/datatypes/buffer.c \
			lib/list/simple-list.c lib/list/lock.c \
			lib/lock/signal.c lib/lock/lockflag.c lib/lock/setflag.c \
			lib/threads/workerthreads.c \
			lib/system/time.c lib/system/path.c lib/system/stat.c lib/system/open.c \
			lib/socket/socket.c lib/socket/common.c lib/socket/connection.c lib/socket/system.c lib/socket/utils.c \
			lib/system/location.c \
			lib/system/fsoperations.c \
			lib/log/logging.c \
			lib/error/error-handling.c \
			lib/network/address.c lib/network/dns.c lib/network/services.c lib/network/utils.c \
			lib/mountinfo/event.c lib/mountinfo/list.c lib/mountinfo/monitor.c lib/mountinfo/read.c lib/mountinfo/mountentry.c lib/mountinfo/utils.c \
			lib/connection/connection.c \
			lib/fuse/mount.c lib/fuse/init.c lib/fuse/receive.c lib/fuse/utils.c  lib/fuse/utils-public.c \
			lib/misc/utils.c

if WITH_LINUX_EPOLL

osns_system_SOURCES += lib/eventloop/backend/linux-epoll.c

else

osns_system_SOURCES += lib/eventloop/backend/glib-mainloop.c 

endif

if GIO2_AVAIL
osns_system_CFLAGS += $(GIO_CFLAGS)
osns_system_CPPFLAGS += $(GIO_CFLAGS)
osns_system_LDADD += $(GIO_LIBS)
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
			ctl/receive.c ctl/send.c ctl/options.c ctl/hashtable.c \
			ctl/read-netcache.c ctl/print-netcache.c ctl/send-netcache.c \
			ctl/send-mountcmd.c \
			osns/pidfile.c osns/receive.c osns/reply.c osns/send.c osns/write.c osns/utils.c \
			lib/eventloop/beventloop.c lib/eventloop/bevent.c lib/eventloop/bevent-write.c \
			lib/eventloop/backend/linux-epoll.c lib/eventloop/beventsubsys.c \
			lib/datatypes/ssh-string.c lib/datatypes/ssh-uint.c lib/datatypes/name-string.c lib/datatypes/buffer.c \
			lib/list/simple-list.c lib/lock/signal.c lib/lock/lockflag.c lib/lock/setflag.c \
			lib/threads/workerthreads.c \
			lib/log/logging.c \
			lib/error/error-handling.c \
			lib/connection/connection.c \
			lib/system/time.c lib/system/path.c lib/system/stat.c lib/system/open.c \
			lib/system/location.c \
			lib/socket/socket.c lib/socket/common.c lib/socket/connection.c lib/socket/system.c lib/socket/utils.c \
			lib/network/utils.c lib/network/address.c lib/network/dns.c \
			lib/misc/utils.c



osns_client_CFLAGS = $(CFLAGS) $(GLIB_CFLAGS) $(LIBGCRYPT_CFLAGS) $(debug_CFLAGS) -D_FILE_OFFSET_BITS=64 -lz -lpthread -fPIC -lm -I . -I include -I ./ssh -I ./lib -Wall -Wno-unused-but-set-variable -Wno-uninitialized -Wno-unused-variable -Wno-unused-label -Wno-unused-function
osns_client_CPPFLAGS = $(CXXFLAGS) $(GLIB_CFLAGS) $(LIBGCRYPT_CFLAGS) $(debug_CFLAGS) -D_FILE_OFFSET_BITS=64 -lz -lpthread -fPIC -lm -I . -I include -I ./ssh -I ./lib -Wall -Wno-unused-but-set-variable -Wno-uninitialized -Wno-unused-variable -Wno-unused-label -Wno-unused-function
osns_client_LDADD = $(GLIB_LIBS) $(LIBGCRYPT_LIBS) $(LDFLAGS)

if BUILD_OSNS_CLIENT
sbin_PROGRAMS += osns_client
endif

osns_client_SOURCES =  	osns_client.c \
			lib/fuse/directory.c lib/fuse/dentry.c lib/fuse/utils.c lib/fuse/receive.c lib/fuse/request.c \
			lib/fuse/fs-common.c lib/fuse/fs-virtual.c lib/fuse/utils-public.c lib/fuse/symlink.c lib/fuse/utils-create.c \
			lib/fuse/fs-service-browse.c lib/fuse/fs-service-path.c lib/fuse/handle.c lib/fuse/opendir.c lib/fuse/openfile.c \
			lib/fuse/path.c lib/fuse/fs-create.c \
			fuse/browse-fs.c fuse/browse/access.c fuse/browse/getattr.c fuse/browse/lookup.c fuse/browse/opendir.c fuse/browse/statfs.c fuse/browse/xattr.c \
			fuse/sftp-fs.c fuse/sftp/access.c fuse/sftp/getattr.c fuse/sftp/inode-stat.c fuse/sftp/lock.c fuse/sftp/lookup.c fuse/sftp/mk.c \
			fuse/sftp/open.c fuse/sftp/opendir.c fuse/sftp/realpath.c fuse/sftp/rm.c fuse/sftp/setattr.c fuse/sftp/statfs.c fuse/sftp/symlink.c fuse/sftp/xattr.c \
			client/arguments.c client/config.c client/workspaces.c client/network.c client/contexes.c client/sftp-context.c client/ssh-context.c client/utils.c \
			lib/resources/create.c lib/resources/localhost.c lib/resources/network.c \
			lib/sl/skiplist.c lib/sl/skiplist-delete.c lib/sl/skiplist-find.c lib/sl/skiplist-insert.c lib/sl/skiplist-utils.c lib/sl/skiplist-lock.c \
			lib/workspace/workspaces.c lib/workspace/utils.c lib/workspace/address.c lib/workspace/inodes.c \
			lib/context/context.c lib/context/init.c lib/context/next.c \
			lib/interface/interface.c lib/interface/iocmd.c lib/interface/list.c \
			lib/misc/utils.c lib/misc/replaceandskip.c \
			osns/client.c osns/control.c osns/hashtable.c osns/init.c osns/mountcmd.c osns/pidfile.c osns/receive.c osns/reply.c osns/send.c osns/utils.c osns/write.c \
			osns/netcache-send.c osns/netcache-read.c osns/record.c osns/setwatch-send.c \
			interface/fuse.c interface/ssh.c interface/ssh-utils.c interface/ssh-channel.c \
			lib/eventloop/beventloop.c lib/eventloop/bevent.c lib/eventloop/bevent-write.c \
			lib/eventloop/beventsignal.c lib/eventloop/beventsubsys.c \
			lib/eventloop/beventtimer.c lib/eventloop/backend/linux-timerfd.c lib/eventloop/backend/linux-signalfd.c \
			lib/datatypes/name.c lib/datatypes/buffer.c lib/datatypes/ssh-string.c lib/datatypes/ssh-uint.c lib/datatypes/name-string.c \
			lib/datatypes/ssh-msg-buffer.c lib/datatypes/ssh-payload.c lib/datatypes/ssh-mpint.c lib/datatypes/ssh-namelist.c \
			lib/list/simple-hash.c lib/list/simple-list.c lib/list/lock.c \
			lib/lock/locking.c lib/lock/lockflag.c lib/lock/setflag.c lib/lock/signal.c \
			lib/threads/workerthreads.c \
			lib/error/error-handling.c \
			lib/log/logging.c \
			lib/system/location.c lib/system/open.c lib/system/path.c lib/system/stat.c lib/system/time.c \
			lib/socket/socket.c lib/socket/common.c lib/socket/connection.c lib/socket/device.c lib/socket/file.c lib/socket/directory.c lib/socket/utils.c \
			lib/connection/connection.c lib/connection/compare.c \
			lib/users/check.c lib/users/getent.c lib/users/local.c lib/users/lookup.c \
			lib/users/mapcb.c lib/users/mapcb_p2l.c lib/users/mapcb_l2p.c lib/users/mapping.c \
			lib/network/dns.c lib/network/utils.c lib/network/address.c lib/network/services.c \
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
			ssh/ssh-common-protocol.c ssh/ssh-common.c ssh/ssh-common-client.c ssh/ssh-signal-client.c \
			ssh/ssh-signal.c ssh/ssh-data.c \
			ssh/ssh-hostinfo.c ssh/ssh-language.c ssh/ssh-pubkey.c ssh/ssh-utils.c \
			ssh/users/mapping.c ssh/users/unknown.c \
			interface/sftp.c interface/sftp-attr.c interface/sftp-send.c interface/sftp-wait-response.c \
			lib/sftp/attr-context.c lib/sftp/attr-utils.c \
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

osns_client_SOURCES += lib/eventloop/backend/linux-epoll.c

else

osns_client_SOURCES += lib/eventloop/backend/glib-mainloop.c 

endif


# if LIBRSYNC_AVAIL
# osns_client_CFLAGS += -lrsync
# osns_client_CPPFLAGS += -lrsync
# osns_client_LDADD += -L/usr/lib64 -lrsync
# endif

if GLIB2_AVAIL
osns_client_CFLAGS += $(GLIB_CFLAGS)
osns_client_CPPFLAGS += $(GLIB_CFLAGS)
osns_client_LDADD += $(GLIB_LIBS)
endif

if GOBJECT2_AVAIL
osns_client_CFLAGS += $(GOBJECT_CFLAGS)
osns_client_CPPFLAGS += $(GOBJECT_CFLAGS)
osns_client_LDADD += $(GOBJECT_LIBS)
endif

if GIO2_AVAIL
osns_client_CFLAGS += $(GIO_CFLAGS)
osns_client_CPPFLAGS += $(GIO_CFLAGS)
osns_client_LDADD += $(GIO_LIBS)
endif

if LIBPAM_AVAIL
osns_client_CFLAGS += -lpam
osns_client_CPPFLAGS += -lpam
else
osns_client_CFLAGS += -lcrypt
osns_client_CPPFLAGS += -lcrypt
endif

# if LIBSMB2_AVAIL
# osns_client_CFLAGS += $(LIBSMB2_CFLAGS)
# osns_client_CPPFLAGS += $(LIBSMB2_CFLAGS)
# osns_client_LDADD += $(LIBSMB2_LIBS)
# endif

if BUILD_SSH_SERVER
sbin_PROGRAMS += osns_server
endif

osns_server_CFLAGS = $(CFLAGS) $(GLIB_CFLAGS) $(LIBGCRYPT_CFLAGS) $(SYSTEMD_CFLAGS) -ggdb -Og  -D_FILE_OFFSET_BITS=64 -lz -lpthread -fPIC -lm -I . -I ./ssh -I ./lib -Wall -Wno-unused-but-set-variable -Wno-uninitialized -Wno-unused-variable -Wno-unused-label -Wno-unused-function
osns_server_CPPFLAGS = $(CXXFLAGS) $(GLIB_CFLAGS) $(LIBGCRYPT_CFLAGS) $(SYSTEMD_CFLAGS)  -ggdb -Og -D_FILE_OFFSET_BITS=64 -lz -lpthread -fPIC -lm -I . -I ./ssh -I ./lib -Wall -Wno-unused-but-set-variable -Wno-uninitialized -Wno-unused-variable -Wno-unused-label -Wno-unused-function
osns_server_LDADD = $(GLIB_LIBS) $(LIBGCRYPT_LIBS) $(SYSTEMD_LIBS) $(LDFLAGS)

osns_server_SOURCES =  	$(osns_common_SOURCES) \
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
osns_server_CFLAGS += $(GLIB_CFLAGS)
osns_server_CPPFLAGS += $(GLIB_CFLAGS)
osns_server_LDADD += $(GLIB_LIBS)
endif

if GOBJECT2_AVAIL
osns_server_CFLAGS += $(GOBJECT2_CFLAGS)
osns_server_CPPFLAGS += $(GOBJECT2_CFLAGS)
osns_server_LDADD += $(GOBJECT2_LIBS)
endif

if GIO2_AVAIL
osns_server_CFLAGS += $(GIO_CFLAGS)
osns_server_CPPFLAGS += $(GIO_CFLAGS)
osns_server_LDADD += $(GIO_LIBS)
endif

if LIBPAM_AVAIL
osns_server_CFLAGS += -lpam
osns_server_CPPFLAGS += -lpam
else
osns_server_CFLAGS += -lcrypt
osns_server_CPPFLAGS += -lcrypt
endif

if BUILD_SFTP_SUBSYSTEM

bin_PROGRAMS += osns_sftp_subsystem

osns_sftp_subsystem_CFLAGS = $(CFLAGS) $(GLIB_CFLAGS) -ggdb -Og  -D_FILE_OFFSET_BITS=64 -lz -lpthread -fPIC -lm -I . -I . -I lib -I include -Wall -Wno-unused-but-set-variable -Wno-uninitialized -Wno-unused-variable -Wno-unused-label -Wno-unused-function
osns_sftp_subsystem_CPPFLAGS = $(CXXFLAGS) $(GLIB_CFLAGS) -ggdb -Og -D_FILE_OFFSET_BITS=64 -lz -lpthread -fPIC -lm -I . -I lib -I include -Wall -Wno-unused-but-set-variable -Wno-uninitialized -Wno-unused-variable -Wno-unused-label -Wno-unused-function
osns_sftp_subsystem_LDADD = $(GLIB_LIBS) $(LDFLAGS)

osns_sftp_subsystem_SOURCES	=	$(osns_common_SOURCES) \
					osns_sftp_subsystem.c ssh/subsystem/connection.c ssh/subsystem/std.c ssh/subsystem/utils.c \
					ssh/subsystem/sftp/attr.c \
					ssh/subsystem/sftp/cb-opendir.c ssh/subsystem/sftp/cb-stat.c \
					ssh/subsystem/sftp/cb-close.c ssh/subsystem/sftp/cb-open.c \
					ssh/subsystem/sftp/cb-readlink.c ssh/subsystem/sftp/cb-rm.c ssh/subsystem/sftp/cb-mk.c \
					ssh/subsystem/sftp/path.c ssh/subsystem/sftp/payload.c \
					ssh/subsystem/sftp/handle.c ssh/subsystem/sftp/init.c \
					ssh/subsystem/sftp/receive.c ssh/subsystem/sftp/send.c \
					ssh/subsystem/sftp/supported.c ssh/subsystem/sftp/extensions.c \
					ssh/subsystem/sftp/extensions/setprefix.c ssh/subsystem/sftp/extensions/statvfs.c \
					ssh/subsystem/sftp/extensions/fsync.c ssh/subsystem/sftp/extensions/mapextension.c \
					lib/sftp/attr-context.c \
					lib/sftp/attr/init-attr-v03.c lib/sftp/attr/init-attr-v04.c lib/sftp/attr/init-attr-v05.c lib/sftp/attr/init-attr-v06.c \
					lib/sftp/attr/read-attr-v03.c lib/sftp/attr/read-attr-v04.c lib/sftp/attr/read-attr-v05.c lib/sftp/attr/read-attr-v06.c \
					lib/sftp/attr/write-attr-v03.c lib/sftp/attr/write-attr-v04.c lib/sftp/attr/write-attr-v05.c lib/sftp/attr/write-attr-v06.c \
					lib/sftp/rw-attr-generic.c \
					lib/users/getent.c \
					lib/users/local.c lib/users/lookup.c lib/users/mapcb.c \
					lib/users/mapcb_l2p.c lib/users/mapcb_p2l.c lib/users/mapping.c \
					lib/lock/signal.c lib/lock/setflag.c lib/lock/lockflag.c \
					osns/pidfile.c lib/misc/utils.c

endif
