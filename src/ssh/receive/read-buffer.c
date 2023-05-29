/*
  2017, 2018 Stef Bon <stefbon@gmail.com>

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
#include "libosns-threads.h"
#include "libosns-misc.h"

#include "ssh-common.h"
#include "ssh-common-protocol.h"
#include "ssh-receive.h"
#include "ssh-utils.h"
#include "ssh-connections.h"

void cb_read_socket_ssh(struct osns_socket_s *sock, uint64_t ctr, void *ptr);

static void cb_read_socket_ssh_thread(void *ptr)
{
    struct ssh_connection_s *sshc=(struct ssh_connection_s *) ptr;
    struct osns_socket_s *sock=&sshc->connection.sock;

    (* sock->ctx.read)(sock, 1, ptr);
}

/*
    pre process:
    function to be called after enough data is present/arrived to begin decrypting the whole packet
*/

/* default pre action:
    - start reading next message when there is one
    this happens when:
    a. decryptor is able to process messages parallel and
    b. not in kexinit/rekey phase
*/

static void cb_post_read_header_parallel(struct ssh_connection_s *sshc)
{
    if (sshc->connection.sock.rd.pos>0) work_workerthread(NULL, 0, cb_read_socket_ssh_thread, (void *) sshc);
}

/* serial pre action
    - do nothing, do not look for the next message cause:
    a. decryptor in not able to process messages in parallel or
    b. in kexinit/rekey phase */

static void cb_post_read_header_ignore(struct ssh_connection_s *sshc)
{
}

/*
    cb pre process message
    function to be called after the whole packet is decrypted
	but before the payload is processed */

static void cb_pre_process_ignore(struct ssh_connection_s *sshc)
{
}

static void cb_pre_process_serial(struct ssh_connection_s *sshc)
{
    if (sshc->connection.sock.rd.pos>0) work_workerthread(NULL, 0, cb_read_socket_ssh_thread, (void *) sshc);
}

/*
    cb post process message
    function to be called after the whole packet is decrypted
	and after the payload is processed */

static unsigned int cb_post_process_ignore(struct ssh_connection_s *sshc, unsigned char type)
{
    return 0;
}

static unsigned int cb_post_process_kex(struct ssh_connection_s *sshc, unsigned char type)
{
    struct osns_socket_s *sock=&sshc->connection.sock;
    struct ssh_receive_s *r=&sshc->receive;
    struct shared_signal_s *signal=&r->signal;
    struct system_timespec_s expire=SYSTEM_TIME_INIT;
    unsigned int errcode=EIO;

    if (type != SSH_MSG_NEWKEYS) return 0;

    get_ssh_connection_expire_init(sshc, &expire);

    if (signal_lock(signal)==0) {

        while (errcode==EIO) {

            if (system_time_test_earlier(&r->kexinit, &r->newkeys)==1) {

                errcode=0;
                break;

            }

	    int result=signal_condtimedwait(signal, &expire);

	    if (result==ETIMEDOUT) {

	        errcode=ETIMEDOUT;
	        break;

	    } else if (sock->status & (SOCKET_STATUS_ERROR | SOCKET_STATUS_CLOSING)) {

	        errcode=ENOTCONN;
	        break;

	    }

        }

        unlockout:
	signal_unlock(signal);

    }

    if (errcode==0) {

        /* go back to the normal mode */

        if (r->decrypt.flags & SSH_DECRYPT_FLAG_PARALLEL) {

	    sshc->pre_process=cb_post_read_header_parallel;
	    sshc->post_process_01=cb_pre_process_ignore;

        } else {

	    sshc->pre_process=cb_post_read_header_ignore;
	    sshc->post_process_01=cb_pre_process_serial;

        }

        sshc->post_process_02=cb_post_process_ignore;
        return 0;

    }

    errorout:
    logoutput_debug("cb_post_process_kex: errcode %u : %s", errcode, strerror(errcode));
    return errcode;
}

/* read data from buffer, decrypt, check mac, and process the packet
    it does this by getting a decryptor; depending the cipher it's possible that more decryptor are in "flight"
*/

void cb_read_socket_ssh(struct osns_socket_s *sock, uint64_t ctr, void *ptr)
{
    struct read_socket_data_s *rd=&sock->rd;
    struct ssh_connection_s *sshc=(struct ssh_connection_s *) ptr;
    struct ssh_session_s *session=get_ssh_connection_session(sshc);
    struct ssh_receive_s *r=&sshc->receive;
    unsigned int errcode=0;
    struct ssh_packet_s packet;

    memset(&packet, 0, sizeof(struct ssh_packet_s));

    while (errcode==0) {
        struct system_timespec_s expire;
        struct ssh_decryptor_s *decryptor=get_decryptor(r, &errcode);
        unsigned int headersize=decryptor->cipher_headersize;
        char tmpheader[headersize];

        get_ssh_connection_expire_init(sshc, &expire);

        signal_lock_flag(&r->signal, &r->status, SSH_RECEIVE_STATUS_THREAD);

        if ((r->threads>0) || (sock->rd.pos==0)) {

            signal_unlock_flag(&r->signal, &r->status, SSH_RECEIVE_STATUS_THREAD);
            (* decryptor->common.queue)(&decryptor->common);
            return;

        }

        r->threads++;
        disable_ssh_socket_read_data(sock);
        signal_unlock_flag(&r->signal, &r->status, SSH_RECEIVE_STATUS_THREAD);

        /* there must be at least a headersize of bytes available to read the length/first block
            if there is not wait for it */

        logoutput_debug("cb_read_socket_ssh: tid %u headersize %u received %u", gettid(), headersize, sock->rd.pos);

        signal_lock(sock->signal);

        while (sock->rd.pos < headersize) {

            int result=signal_condtimedwait(sock->signal, &expire);

            if (result==ETIMEDOUT) {

                signal_unlock(sock->signal);
                errcode=ETIMEDOUT;
                signal_unset_flag(&r->signal, &r->status, SSH_RECEIVE_STATUS_THREAD);
                goto dooutbreak;

            } else if (sock->status & (SOCKET_STATUS_ERROR | SOCKET_STATUS_CLOSING)) {

                signal_unlock(sock->signal);
                errcode=ENOTCONN;
                signal_unset_flag(&r->signal, &r->status, SSH_RECEIVE_STATUS_THREAD);
                goto dooutbreak;

            }

        }

        signal_unlock(sock->signal);

        readpacket:

        packet.len=0;
        packet.size=0;
        packet.padding=0;
        packet.error=0;
        packet.type=0;
        packet.decrypted=0;
        packet.sequence=r->sequence_number;
        packet.buffer=sock->rd.buffer;

        r->sequence_number++;
        logoutput_debug("cb_read_socket_ssh: tid %u seq %u rawdata pos %u", gettid(), packet.sequence, sock->rd.pos);

        /* decrypt first block to get the packet length
	    don't decrypt inplace cause it's possible that hmac is verified over the encrypted text
	    in stead store the decrypted header in a seperate temporary buffer
	    and copy that buffer later back when the whole packet is decrypted */

        if ((* decryptor->decrypt_length)(decryptor, &packet, tmpheader, headersize)==0) {
	    unsigned int msgsize=0;

	    packet.len=get_uint32(tmpheader);
	    msgsize=packet.len + 4 + decryptor->hmac_maclen; /* total number of bytes to expect of binary ssh message ... here some check for range ? */

            if (msgsize >= session->config.max_packet_size) {

                logoutput_debug("cb_read_socket_ssh: tid %u seq %u packet size %u received %u ... packet size (%u) too large (hmaclen %u)", gettid(), packet.sequence, msgsize, sock->rd.pos, packet.len, decryptor->hmac_maclen);
                errcode=EIO;
                signal_unset_flag(&r->signal, &r->status, SSH_RECEIVE_STATUS_THREAD);
                goto dooutbreak;

            }

	    logoutput_debug("cb_read_socket_ssh: tid %u seq %u packet size %u received %u (hmaclen %u)", gettid(), packet.sequence, msgsize, sock->rd.pos, decryptor->hmac_maclen);
            char data[msgsize];

	    /* if not read enough wait for more ... at least size bytes have to in the buffer */

            signal_lock(sock->signal);

            r->msgsize=msgsize;

            while (sock->rd.pos < msgsize) {

                int result=signal_condtimedwait(sock->signal, &expire);

                if (result==ETIMEDOUT) {

                    signal_unlock(sock->signal);
                    errcode=ETIMEDOUT;
                    signal_unset_flag(&r->signal, &r->status, SSH_RECEIVE_STATUS_THREAD);
                    goto dooutbreak;

                } else if (sock->status & (SOCKET_STATUS_ERROR | SOCKET_STATUS_CLOSING)) {

                    signal_unlock(sock->signal);
                    errcode=ENOTCONN;
                    signal_unset_flag(&r->signal, &r->status, SSH_RECEIVE_STATUS_THREAD);
                    goto dooutbreak;

                }

            }

            memcpy(data, sock->rd.buffer, msgsize);
            packet.buffer=data;

            r->msgsize=0;

            if (sock->rd.pos == msgsize) {

                sock->rd.pos=0;

            } else {
                char *buffer=sock->rd.buffer;
                unsigned int bytesleft=(sock->rd.pos - msgsize);

                /* more data received than the current message */

                logoutput_debug("cb_read_socket_ssh: tid %u seq %u packet size %u bytesleft %u", gettid(), packet.sequence, msgsize, bytesleft);
                memmove(buffer, &buffer[msgsize], bytesleft);
                sock->rd.pos=bytesleft;

            }

            signal_unlock(sock->signal);
            enable_ssh_socket_read_data(sock);
            r->threads--;
            signal_unset_flag(&r->signal, &r->status, SSH_RECEIVE_STATUS_THREAD);

            (* sshc->pre_process)(sshc);

	    /* now enough data to call it a packet ... */

	    /* do mac checking when "before decrypting" is used: use the encrypted data
	    in other cases ("do mac checking after decryption") this does nothing
	    the "after decryption" mode is the default, and described in
	    RFC 4253 The Secure Shell (SSH) Transport Layer Transport Protocol 6.4 Data Intergrity */

            errcode=EPROTO;

	    if ((* decryptor->verify_hmac_pre)(decryptor, &packet)==0) {

	        memcpy(packet.buffer, tmpheader, headersize);

	        /* decrypt rest */

	        if ((* decryptor->decrypt_packet)(decryptor, &packet)==0) {

		    packet.padding=(unsigned char) *(packet.buffer + 4);

		    /* do mac checking when "after decryption" is used */

		    if ((* decryptor->verify_hmac_post)(decryptor, &packet)==0) {
		        struct ssh_decompressor_s *decompressor=NULL;
		        struct ssh_payload_s *payload=NULL;

		        /* ready with decryptor */
		        (* decryptor->common.queue)(&decryptor->common);
		        decryptor=NULL;
		        (* sshc->post_process_01)(sshc);

                        decompressor=get_decompressor(r, &errcode);
                        payload=(* decompressor->decompress_packet)(decompressor, &packet, &errcode);
                        packet.buffer=NULL;

                        if (payload) {
                            unsigned char type=payload->type;

                            process_cb_ssh_payload(sshc, payload);
                            (* decompressor->common.queue)(&decompressor->common);
		            errcode=(* sshc->post_process_02)(sshc, type);

                        } else {

                            logoutput_warning("cb_read_socket_ssh: unable to allocate payload");
                            (* decompressor->common.queue)(&decompressor->common);
                            if (errcode==0) errcode=ENOMEM;

                        }

		    }

	        }

	    }

        }

        dooutbreak:
        if (decryptor) (* decryptor->common.queue)(&decryptor->common);

    }

    outcheck:

    if (errcode==EIO || errcode==ENOTCONN || errcode==ETIMEDOUT || errcode==ENOMEM) {

        logoutput_warning("cb_read_socket_ssh: tid %u ignoring received data errcode %u (%s)", gettid(), errcode, strerror(errcode));
        disconnect_ssh_connection(sshc);

    }

}

void set_ssh_receive_behaviour(struct ssh_connection_s *sshc, const char *phase)
{

    logoutput_debug("set_ssh_receive_behaviour: set phase %s", phase);

    if ((strcmp(phase, "greeter")==0) || (strcmp(phase, "session")==0)) {

	sshc->pre_process=cb_post_read_header_ignore;
	sshc->post_process_01=cb_pre_process_serial;
	sshc->post_process_02=cb_post_process_ignore;

    } else if (strcmp(phase, "kexinit")==0) {

	sshc->receive.status |= SSH_RECEIVE_STATUS_KEXINIT;
	sshc->receive.status &= ~SSH_RECEIVE_STATUS_NEWKEYS;
	get_current_time_system_time(&sshc->receive.kexinit);
	sshc->pre_process=cb_post_read_header_ignore;
	sshc->post_process_01=cb_pre_process_ignore;
	sshc->post_process_02=cb_post_process_kex;

    } else if (strcmp(phase, "newkeys")==0) {

	sshc->receive.status &= ~SSH_RECEIVE_STATUS_KEXINIT;
	sshc->receive.status |= SSH_RECEIVE_STATUS_NEWKEYS;
	get_current_time_system_time(&sshc->receive.newkeys);

    }

}
