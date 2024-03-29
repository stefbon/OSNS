# OSNS
Open Secure Network Services

INTRODUCTION
============

This fuse service provides automatic access to network services like sftp over ssh.
Sftp hosts providing the ssh/sftp service are detected (using avahi) and
fs-workspace tries to establish a connection using the keys available for the user.
(found in ~/.ssh).

The mountpoint for network services looks like:

/run/network/$USER/fs

this gives a FUSE fs like:

/run/network/$USER/fs/Open Secure Network/example.org/server/home

are created to offer access to the user's home directory on server when
the server server.example.org is detected on the network as a host providing ssh/sftp.

Own ssh and sftp implementations are written espacially for this. The reason for this is that
existing ssh libraries do not offer the required integration with a context like this. Pending sftp requests
are waiting for the following signals:

- of course the regular response
- the response is invalid (=protocol error)
- the original FUSE request is interrupted, so the related sftp request has to be cancelled and the response from server ignored (=interrupt)
- the remote server waits too long to respond, a timeout occurs (= timeout)
- the remote server closes the sftp subsystem, the related ssh channel and/or ssh session (= no connection)
- the sftp filesystem is "unmounted" by the local system (= cancel)

The internal SSH server supports publickey and hostbased userauth, rsa and dss key formats,
key exchange methods diffie-hellman-group1-sha1 and diffie-hellman-group14-sha1,
ciphers chacha20-poly1305@openssh.com, aes128-cbc and aes128-ctr, aes256-cbc and aes256-ctr,
hmac hmac-sha1, hmac-sha256 and hmac-md5. It looks at the regular locations like $HOME/.ssh and /etc/ssh for pubic and private keys.

SFTP supports versions 3-6.


REQUIREMENTS
============

For crypto libgcrypt is used.
For userlogins systemd is used.
For detection of hosts and networkservices avahi is used.
Note libfuse is not required, fuse support in the kernel of course.


FEATURES
========

- Automatic detection of services on the local network.
- Atomatic mounting of filesystem providing access to sftp/ssh servers on the network.
- Flexible handling of different signals/events like remote closing channel, interrupt requests and unmounting.
- SSH and SFTP subsystems uses own implementation to have the best integration
- written inc, with readability in mind, All the complicated details doing verify, sign, de- and encryption go in specific files.
You won't find any of this in the mainline of the program.
- userauth and publickey authentication is supported
- sftp protocol versions 3 to 6 are supported
- 

BUILD
=====

Create directory to get the source:

git clone git@github.com:stefbon/OSNS.git
cd OSNS
cd src

Run the autogen.sh script to create the different buildfiles:

./autogen.sh

Run the configure script, and make:

./configure
make

This will build the osns_client executable.

As root:

Copy the options file from source/workspace
Adjust the options file to you needs. Some important options:
- the policy to allow user to make use of mounting of remote services ("user.network_mount_group_policy").
  Two choices are:
    - partof: user is partof a group. This maybe the primary group like "users", but also a secondary like "fuse-netmount" for example.
    - min: the gid of users primary group is minimal some value
- the group to allow mounting ("user.network_mount_group").
- the mountpoint. Default /run/network/$USER ("user.mount_template").

Other options:
- the name chosen for the remote home folder: home or the remote username ("sftp.network.home_use_remotename").
- the domainname is created in the browseable network map yes or no ("sftp.network.show_domainname").

Copy the desktopfiles in config to /etc/fs-workspace:

cp desktopentry.* /etc/fs-workspace
This is not required, but gives nice icons for the domain and the server.
I'm using the network-workgroup and network-server icons from Adwaita icons collection.

Start the executable

./osns_client


Other filesystems
=================

Other filesystems than sftp are possible, for example NFS and SMB, using libnfs and libsmb2 (SMB2/SMB3) of R. Sahlberg. This library has a nice api
and are also path based.
See:
- https://github.com/sahlberg/libsmb2
- https://github.com/sahlberg/libnfs


TODO
====

- support for ed25519 (in public/private keys and key exchange curve 25519)
at this moment rsa and dss are supported, and none elyptic curve based key exchange methods.
- support for backup. Add a "share" per server special for backups. The sftp protocol gives room to add extra calls, for example to
make a backup. Using librsync would be a good idea here.
- support for a UDP channel (like MOSH) for fast data transfer (using libudt)
- support for forwarding of ports like CUPS socket to secure access the remote printer server
- support of a chat terminal per server, allowing users per domain to chat public or private. Also providing an overview of users in
/run/network/$USER/example.org/chat
- support for more key providers than OPENSSH (=local files, like ~/.ssh/id_rsa), like a key deamon and a usb device like NitroKey.
- not only client but also a dedicated fileserver providing services like video and/or textchat (public and private), fsnotify over the network. Make use of the
SSH_MSG_GLOBAL_REQUEST to have a custom request like "enumservices@sons.org" and "getservice@sons.org".
- integrate with shared users databases like openldap
- integrate with a central Certificate Authority (CA) to make things work and doable with a lot of users


USEFULL INFO
============

SSH:

https://tools.ietf.org/html/rfc4250

https://tools.ietf.org/html/rfc4251

https://tools.ietf.org/html/rfc4252

https://tools.ietf.org/html/rfc4253

https://tools.ietf.org/html/rfc4254

Extension Negotiation

https://tools.ietf.org/html/rfc8308

Elliptic Curve Algorithm

https://tools.ietf.org/html/rfc5656 (generic)

https://tools.ietf.org/html/rfc8709 (ed25519 and ed448)

SHA-2 Data Integrity Verification

https://tools.ietf.org/html/rfc6668

Use of RSA Keys with SHA-256 and SHA-512

https://tools.ietf.org/html/rfc8332

Key exchange method curve25519-sha256@libssh.org by libssh.

https://git.libssh.org/projects/libssh.git/tree/doc/curve25519-sha256@libssh.org.txt

Cipher and hmac chacha20poly1305@openssh.com

https://cvsweb.openbsd.org/src/usr.bin/ssh/PROTOCOL.chacha20poly1305?annotate=HEAD

Extensions by OPENSSH

https://cvsweb.openbsd.org/src/usr.bin/ssh/PROTOCOL?annotate=HEAD

SFTP:

https://tools.ietf.org/html/draft-ietf-secsh-filexfer-13

LIBGCRYPT:

https://www.gnupg.org/documentation/manuals/gcrypt/index.html

Introduction to Cryptography by Christof Paar:
(very very usefull! an absolute must)

https://www.youtube.com/channel/UC1usFRN4LCMcfIV7UjHNuQg/featured

Last but not leasT, ververy usefull information about different sftp implementations:

https://www.greenend.org.uk/rjk/sftp/sftpimpls.html

https://www.greenend.org.uk/rjk/sftp/sftpversions.html

Especially the latest is usefull. It gives an oversight of the properties and changes in the different sftp protocol versions,
and a table of the documentation versus protocol. Very very usefull!

AVAHI:

For network service discovery:

https://www.avahi.org/doxygen/html/index.html

