/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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
#include "libosns-threads.h"
#include "libosns-interface.h"

#include "ssh-utils.h"

#include "ssh-common-protocol.h"
#include "ssh-common.h"
#include "ssh-connections.h"
#include "ssh-receive.h"
#include "ssh-userauth.h"
#include "ssh-signal.h"

static void clear_ssh_setup(struct ssh_setup_s *setup)
{

    if (setup->status==SSH_SETUP_PHASE_TRANSPORT) {

	if (setup->phase.transport.status==SSH_TRANSPORT_TYPE_KEX) {
	    struct ssh_keyexchange_s *kex=&setup->phase.transport.type.kex;

	    clear_ssh_string(&kex->kexinit_client);
	    clear_ssh_string(&kex->kexinit_server);
	    clear_ssh_string(&kex->cipher_key_c2s);
	    clear_ssh_string(&kex->cipher_iv_c2s);
	    clear_ssh_string(&kex->hmac_key_c2s);
	    clear_ssh_string(&kex->cipher_key_s2c);
	    clear_ssh_string(&kex->cipher_iv_s2c);
	    clear_ssh_string(&kex->hmac_key_s2c);

	}

    } else if (setup->status==SSH_SETUP_PHASE_SERVICE) {

	if (setup->phase.service.status==SSH_SERVICE_TYPE_AUTH) {
	    struct ssh_auth_s *auth=&setup->phase.service.type.auth;

	    clear_ssh_auth(auth);

	}

    }

}

static int check_flag_bits(unsigned int flags, unsigned int flag)
{
    return (((flags & flag) == flag) ? 1 : 0);
}

void finish_ssh_connection_setup(struct ssh_connection_s *connection, const char *phase, unsigned int type)
{
    struct ssh_setup_s *setup=&connection->setup;

    signal_lock(setup->signal);

    if (strcmp(phase, "transport")==0) {

	if (setup->status!=SSH_SETUP_PHASE_TRANSPORT) return;

	if (type==SSH_TRANSPORT_TYPE_GREETER) {

	    setup->flags |= SSH_SETUP_FLAG_GREETER;

	} else if (type==SSH_TRANSPORT_TYPE_KEX) {

	    setup->flags |= SSH_SETUP_FLAG_KEX;

	} else if (type==0) {

	    setup->flags |= SSH_SETUP_FLAG_TRANSPORT;
	    clear_ssh_setup(setup);
	    setup->status=0;

	}

    } else if (strcmp(phase, "service")==0) {

	if (setup->status!=SSH_SETUP_PHASE_SERVICE) return;

	if (type==SSH_SERVICE_TYPE_AUTH) {

	    setup->flags |= SSH_SETUP_FLAG_SERVICE_AUTH;

	} else if (type==0) {

	    setup->flags |= SSH_SETUP_FLAG_SERVICE_CONNECTION;
	    clear_ssh_setup(setup);
	    setup->status=0;

	}

    } else if (strcmp(phase, "setup")==0) {

	if (type==0) {

	    if (setup->flags & SSH_SETUP_FLAG_SETUPTHREAD) {
		pthread_t thread=pthread_self();

		if (pthread_equal(setup->thread, thread)) {

		    setup->thread=0;
		    setup->flags-=SSH_SETUP_FLAG_SETUPTHREAD;

		}

	    }

	}

    } else {

	clear_ssh_setup(setup);

    }

    signal_broadcast(setup->signal);
    signal_unlock(setup->signal);

}

void init_ssh_connection_setup(struct ssh_connection_s *connection, const char *phase, unsigned int type)
{
    struct ssh_setup_s *setup=&connection->setup;

    if (strcmp(phase, "init")==0) {

	init_payload_queue(connection, &setup->queue);
	setup->status=0;
	setup->flags=0;
	setup->error=0;

    } else if (strcmp(phase, "transport")==0) {

	if (setup->status==SSH_SETUP_PHASE_TRANSPORT && setup->phase.transport.status==type) {

	    logoutput("init_connection_setup: error, setup already in transport:%i", type);
	    return;

	}

	clear_ssh_setup(setup);

	if (type==SSH_TRANSPORT_TYPE_GREETER) {
	    struct ssh_greeter_s *greeter=&setup->phase.transport.type.greeter;

	    greeter->flags=0;
	    setup->phase.transport.status=type;
	    setup->status=SSH_SETUP_PHASE_TRANSPORT;
	    if (setup->flags & SSH_SETUP_FLAG_GREETER) setup->flags -= SSH_SETUP_FLAG_GREETER;

	} else if (type==SSH_TRANSPORT_TYPE_KEX) {
	    struct ssh_keyexchange_s *kex=&setup->phase.transport.type.kex;

	    init_ssh_string(&kex->kexinit_server);
	    init_ssh_string(&kex->kexinit_client);
	    kex->algos=NULL;
	    for (unsigned int i=0; i<SSH_ALGO_TYPES_COUNT; i++) kex->chosen[i]=-1;
	    init_ssh_string(&kex->cipher_key_c2s);
	    init_ssh_string(&kex->cipher_iv_c2s);
	    init_ssh_string(&kex->hmac_key_c2s);
	    init_ssh_string(&kex->cipher_key_s2c);
	    init_ssh_string(&kex->cipher_iv_s2c);
	    init_ssh_string(&kex->hmac_key_s2c);
	    kex->flags=0;
	    setup->phase.transport.status=type;
	    setup->status=SSH_SETUP_PHASE_TRANSPORT;
	    if (setup->flags & SSH_SETUP_FLAG_KEX) setup->flags -= SSH_SETUP_FLAG_KEX;

	}

    } else if (strcmp(phase, "service")==0) {

	if (setup->status==SSH_SETUP_PHASE_SERVICE && setup->phase.service.status==type) {

	    logoutput("init_connection_setup: error, setup already in service:%i", type);
	    return;

	}

	clear_ssh_setup(setup);

	if (type==SSH_SERVICE_TYPE_AUTH) {
	    struct ssh_auth_s *auth=&setup->phase.service.type.auth;

	    init_ssh_auth(auth);
	    setup->phase.service.status=type;
	    setup->status=SSH_SETUP_PHASE_SERVICE;
	    if (setup->flags & SSH_SETUP_FLAG_SERVICE_AUTH) setup->flags -= SSH_SETUP_FLAG_SERVICE_AUTH;

	}

    } else if (strcmp(phase, "clear")==0) {

	clear_ssh_setup(setup);
	clear_payload_queue(&setup->queue, 1);
	setup->status=0;

    } else if (strcmp(phase, "free")==0) {

	clear_ssh_setup(setup);
	clear_payload_queue(&setup->queue, 1);
	setup->signal=NULL;
	memset(setup, 0, sizeof(struct ssh_setup_s));

    }

}

int change_ssh_connection_setup(struct ssh_connection_s *connection, const char *phase, unsigned int type, unsigned int flag, unsigned int option,
			    int (* setup_cb)(struct ssh_connection_s *c, void *d), void *data)
{
    struct ssh_setup_s *setup=&connection->setup;
    int result=0;

    logoutput_debug("change_ssh_connection_setup: phase %s type %i flag %i option %i", phase, type, flag, option);

    signal_lock(setup->signal);

    if (phase==NULL || strlen(phase)==0) {
	unsigned int all=(SSH_SETUP_FLAG_GREETER|SSH_SETUP_FLAG_KEX|SSH_SETUP_FLAG_TRANSPORT|SSH_SETUP_FLAG_SERVICE_AUTH);

	flag &= all;
	setup->flags |= flag;
	if (setup_cb) result=setup_cb(connection, data);

    } else if (strcmp(phase, "transport")==0) {

	if (setup->status!=SSH_SETUP_PHASE_TRANSPORT || setup->phase.transport.status!=type) {

	    result=-1;
	    goto out;

	}

	if (type==SSH_TRANSPORT_TYPE_GREETER) {
	    struct ssh_greeter_s *greeter=&setup->phase.transport.type.greeter;
	    unsigned int all=(SSH_GREETER_FLAG_C2S|SSH_GREETER_FLAG_S2C);

	    flag &= all;
	    if (flag & all) {
		unsigned int common=(greeter->flags & flag);

		if (common) {

		    if (option & SSH_SETUP_OPTION_XOR) {

			result=-1;
			goto out;

		    } else if (option & SSH_SETUP_OPTION_UNDO) {

			greeter->flags &= ~common;
			flag &= ~common;

		    }

		}

		greeter->flags |= flag;
		if (setup_cb) result=setup_cb(connection, data);

	    }

	} else if (type==SSH_TRANSPORT_TYPE_KEX) {
	    struct ssh_keyexchange_s *kex=&setup->phase.transport.type.kex;
	    unsigned int all=(SSH_KEX_FLAG_KEXINIT_C2S|SSH_KEX_FLAG_KEXINIT_S2C|SSH_KEX_FLAG_KEXDH_C2S|
		    SSH_KEX_FLAG_KEXDH_S2C|SSH_KEX_FLAG_NEWKEYS_C2S|SSH_KEX_FLAG_NEWKEYS_S2C);

	    flag &= all;

	    if (flag & all) {
		unsigned int common=(kex->flags & flag);

		if (common) {

		    if (option & SSH_SETUP_OPTION_XOR) {

			result=-1;
			goto out;

		    } else if (option & SSH_SETUP_OPTION_UNDO) {

			/* undo only the common bits */

			kex->flags &= ~common;
			flag &= ~common;

		    }

		}

		kex->flags |= flag;
		if (setup_cb) result=setup_cb(connection, data);

	    }

	}

    } else if (strcmp(phase, "service")==0) {

	if (setup->status!=SSH_SETUP_PHASE_SERVICE || setup->phase.service.status!=type) {

	    result=-1;
	    goto out;

	}

	if (type==SSH_SERVICE_TYPE_AUTH) {
	    struct ssh_auth_s *auth=&setup->phase.service.type.auth;
	    unsigned int all=(SSH_AUTH_METHOD_NONE|SSH_AUTH_METHOD_PUBLICKEY|SSH_AUTH_METHOD_PASSWORD|SSH_AUTH_METHOD_HOSTBASED);

	    flag &= all;
	    if (flag & all) {
		unsigned int common=(auth->done & flag);

		if (common) {

		    if (option & SSH_SETUP_OPTION_XOR) {

			result=-1;
			goto out;

		    } else if (option & SSH_SETUP_OPTION_UNDO) {

			/* undo only the common bits */

			auth->done &= ~common;
			flag &= ~common;

		    }

		}

		auth->done |= flag;
		if (setup_cb) result=setup_cb(connection, data);

	    }

	}

    } else if (strcmp(phase, "setup")==0) {
	unsigned int all=(SSH_SETUP_FLAG_DISCONNECT|SSH_SETUP_FLAG_ANALYZETHREAD|SSH_SETUP_FLAG_SETUPTHREAD|SSH_SETUP_FLAG_HOSTINFO);

	flag &= all;

	/* if already disconnected not going disconnecting again */

	if ((setup->flags & SSH_SETUP_FLAG_DISCONNECT) && (flag & SSH_SETUP_FLAG_DISCONNECTING)) flag &= ~SSH_SETUP_FLAG_DISCONNECTING;

	if (flag & all) {
	    unsigned int common=(setup->flags & flag);

	    if (common) {

		if (option & SSH_SETUP_OPTION_XOR) {

		    result=-1;
		    goto out;

		} else if (option & SSH_SETUP_OPTION_UNDO) {

		    /* undo only the common bits */

		    setup->flags &= ~common;
		    flag &= ~common;

		}

	    }

	    if (flag & SSH_SETUP_FLAG_SETUPTHREAD) setup->thread=pthread_self();
	    if (flag & SSH_SETUP_FLAG_DISCONNECTED) setup->flags &= ~SSH_SETUP_FLAG_DISCONNECTING;

	    setup->flags|=flag;
	    if (setup_cb) result=setup_cb(connection, data);

	} else {

	    /* flag not set */
	    result=-1;

	}

    }

    out:

    signal_broadcast(setup->signal);
    signal_unlock(setup->signal);
    return result;

}

int check_ssh_connection_setup(struct ssh_connection_s *connection, const char *phase, unsigned int type, unsigned int flag)
{
    struct ssh_setup_s *setup=&connection->setup;
    int result=-1;

    if (phase==NULL || strlen(phase)==0) {
	unsigned int all=(SSH_SETUP_FLAG_GREETER|SSH_SETUP_FLAG_KEX|SSH_SETUP_FLAG_TRANSPORT|SSH_SETUP_FLAG_SERVICE_AUTH);

	flag=(flag==0) ? all : (flag & all);
	result=check_flag_bits(setup->flags, flag);

    } else if (strcmp(phase, "transport")==0) {

	if (setup->status==SSH_SETUP_PHASE_TRANSPORT && setup->phase.transport.status==type) {

	    if (type==SSH_TRANSPORT_TYPE_GREETER) {
		struct ssh_greeter_s *greeter=&setup->phase.transport.type.greeter;
		unsigned int all=(SSH_GREETER_FLAG_C2S|SSH_GREETER_FLAG_S2C);

		if (flag==0) flag=all;
		result=check_flag_bits(greeter->flags, flag);

	    } else if (type==SSH_TRANSPORT_TYPE_KEX) {
		struct ssh_keyexchange_s *kex=&setup->phase.transport.type.kex;
		unsigned int all=(SSH_KEX_FLAG_KEXINIT_C2S|SSH_KEX_FLAG_KEXINIT_S2C|SSH_KEX_FLAG_KEXDH_C2S|
		    SSH_KEX_FLAG_KEXDH_S2C|SSH_KEX_FLAG_NEWKEYS_C2S|SSH_KEX_FLAG_NEWKEYS_S2C);

		if (flag==0) flag=all;
		result=check_flag_bits(kex->flags, flag);

	    }

	}

    } else if (strcmp(phase, "service")==0) {

	if (setup->status==SSH_SETUP_PHASE_SERVICE && setup->phase.service.status==type) {

	    if (type==SSH_SERVICE_TYPE_AUTH) {
		struct ssh_auth_s *auth=&setup->phase.service.type.auth;
		unsigned int all=(SSH_AUTH_METHOD_NONE|SSH_AUTH_METHOD_PUBLICKEY|SSH_AUTH_METHOD_PASSWORD|SSH_AUTH_METHOD_HOSTBASED);

		flag &= all;

		if (flag==0) {

		    result=check_flag_bits(auth->done, auth->required);

		} else {

		    flag &= all;
		    result=check_flag_bits(auth->done, flag);

		}

	    }

	}

    } else if (strcmp(phase, "setup")==0) {
	unsigned int all=(SSH_SETUP_FLAG_DISCONNECT|SSH_SETUP_FLAG_ANALYZETHREAD|SSH_SETUP_FLAG_SETUPTHREAD|SSH_SETUP_FLAG_HOSTINFO);

	if (flag==0) flag=all;
	result=check_flag_bits(setup->flags, flag);

    }

    return result;

}

int wait_ssh_connection_setup_change(struct ssh_connection_s *connection, const char *phase, unsigned int type, unsigned int flag, int (* setup_cb)(struct ssh_connection_s *connection, void *data), void *data)
{
    struct ssh_setup_s *setup=&connection->setup;
    int result=0;

    signal_lock(setup->signal);

    result=check_ssh_connection_setup(connection, phase, type, flag);

    while (result==0) {

	signal_condwait(setup->signal);
	result=check_ssh_connection_setup(connection, phase, type, flag);
	if (result==1 && setup_cb) (* setup_cb)(connection, data); 

    }

    signal_unlock(setup->signal);

    return (result==1) ? 0 : -1;
}

