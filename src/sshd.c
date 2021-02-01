/*
    tel2com

    Copyright (c) 2018 Nanshiki Corporation

    This software is released under the MIT License.
    https://opensource.org/licenses/mit-license.php
*/
#include <libssh/callbacks.h>
#include <libssh/server.h>
#include <poll.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <stdio.h>
#include <termios.h>
#include <pthread.h>
#include <crypt.h>

#include "t2c_def.h"
#include "tel2com.h"
#include "sshd.h"
#include "telnet.h"
#include "uart.h"

enum {
	modeNone,

	modeCheckAuth,
	modeConnected,
	modeDisconnect,
};

struct SSHD_SESSION_DATA {
	struct SSHD_SESSION_DATA *next;
	int mode;
	int auth_flag;
	int close_flag;
	int clear_flag;
	int socket;
	ssh_session session;
	ssh_event event;
	ssh_channel channel;
	struct ssh_channel_callbacks_struct channel_cb;
	struct ssh_server_callbacks_struct server_cb;
	struct COM_DATA *com;
	char *enter_id;
	char *enter_pass;
};

struct SSHD_DATA {
	ssh_bind bind; 
	int listen_socket;
	struct SSHD_SESSION_DATA *session_data;
	int active_flag;
	int auth_mode;
	char *user;
	char *pass;
	char *pub_key;
};

static struct SSHD_DATA sshd_data;

#define	SALT_METHOD			"$6$"
#define	SALT_METHOD_LENGTH	3
#define	SALT_LENGTH			16
static const char *hash_string = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static void make_salt(char *salt)
{
	int fd;
	unsigned char salt_key[SALT_LENGTH + 1];
	int no, len = strlen(hash_string);

	if((fd = open("/dev/urandom", O_RDONLY)) != -1) {
		read(fd, salt_key, SALT_LENGTH);
		close(fd);

		strcpy(salt, SALT_METHOD);
		for(no = 0 ; no < SALT_LENGTH ; no++) {
			salt[SALT_METHOD_LENGTH + no] = hash_string[salt_key[no] % len];
		}
		salt[SALT_METHOD_LENGTH + SALT_LENGTH] = '$';
		salt[SALT_METHOD_LENGTH + SALT_LENGTH + 1] = '\0';
	}
}

char *create_password_hash(char *pass)
{
	char salt[SALT_METHOD_LENGTH + SALT_LENGTH + 2];

	make_salt(salt);

	return crypt(pass, salt);
}

static int check_password_hash(const char *pass, const char *hash)
{
	char *tail;

	if((tail = strrchr(hash, '$')) != NULL) {
		int len = tail - hash;
		if(len < SALT_METHOD_LENGTH + SALT_LENGTH + 1) {
			char salt[len + 1];
			char *make_hash;
			strncpy(salt, hash, len + 1);
			salt[len + 1] = '\0';
			make_hash = crypt(pass, salt);
			return strcmp(hash, make_hash);
		}
	}
	return -1;
}

#ifdef ENABLE_SSHD_LOG
static void log_function(int priority, const char *function, const char *buffer, void *userdata)
{
	printf("%d:%s:%s\n", priority, function, buffer);
}
#endif

static int data_function(ssh_session session, ssh_channel channel, void *data, uint32_t len, int is_stderr, void *userdata)
{
	struct SSHD_SESSION_DATA *session_data = (struct SSHD_SESSION_DATA *)userdata;
	if(session_data->com) {
		if(session_data->com->handle != -1) {
			session_data->com->send_id_flag = 1;
			session_data->com->send_pass_flag = 1;
			tcdrain(session_data->com->handle);
			write(session_data->com->handle, data, len);
			gettimeofday(&session_data->com->no_comm_start, NULL);
		}
	}
	return len;
}

static int auth_password(ssh_session session, const char *user, const char *pass, void *userdata)
{
	struct SSHD_SESSION_DATA *session_data = (struct SSHD_SESSION_DATA *)userdata;
	if(sshd_data.auth_mode == authModeBBS) {
		session_data->auth_flag = 1;
		if(session_data->enter_id) {
			free(session_data->enter_id);
		}
		session_data->enter_id = strdup(user);
		if(session_data->enter_pass) {
			free(session_data->enter_pass);
		}
		session_data->enter_pass = strdup(pass);
		return SSH_AUTH_SUCCESS;
	} else if(strcmp(user, sshd_data.user) == 0 && check_password_hash(pass, sshd_data.pass) == 0) {
		session_data->auth_flag = 1;
		return SSH_AUTH_SUCCESS;
	} 
	return SSH_AUTH_DENIED;
} 

static ssh_channel channel_open(ssh_session session, void *userdata)
{ 
	struct SSHD_SESSION_DATA *session_data = (struct SSHD_SESSION_DATA *)userdata;
	session_data->channel = ssh_channel_new(session);
	return session_data->channel;
}

static int pty_request(ssh_session session, ssh_channel channel, const char *term, int cols, int rows, int py, int px, void *userdata)
{
	return SSH_OK;
}

static int shell_request(ssh_session session, ssh_channel channel, void *userdata)
{
	return SSH_OK;
}

static int auth_publickey(ssh_session session, const char *user, struct ssh_key_struct *pubkey, char signature_state, void *userdata)
{
	struct SSHD_SESSION_DATA *session_data = (struct SSHD_SESSION_DATA *)userdata;
	int ret = SSH_AUTH_DENIED;

	if(signature_state == SSH_PUBLICKEY_STATE_NONE) {
		return SSH_AUTH_SUCCESS;
	}
	if(signature_state != SSH_PUBLICKEY_STATE_VALID) {
		return SSH_AUTH_DENIED;
	}
	if(sshd_data.pub_key) {
		FILE *fp;
		if((fp = fopen(sshd_data.pub_key, "r")) != NULL) {
			char line[RECEIVE_BUFFER_LENGTH];
			char *item;
			while(fgets(line, RECEIVE_BUFFER_LENGTH, fp) != NULL) {
				if((item = strtok(line, " ")) != NULL) {
    				enum ssh_keytypes_e type = ssh_key_type_from_name(item);
    				if(type != SSH_KEYTYPE_UNKNOWN) {
						if((item = strtok(NULL, " ")) != NULL) {
							ssh_key key = NULL;
							int result = ssh_pki_import_pubkey_base64(item, type, &key);
							if(result == SSH_OK && key != NULL) {
								result = ssh_key_cmp(key, pubkey, SSH_KEY_CMP_PUBLIC);
								ssh_key_free(key);
								if(result == SSH_OK) {
									session_data->auth_flag = 1;
									ret = SSH_AUTH_SUCCESS;
									break;
								}
							}
						}
					}
				}
			}
			fclose(fp);
		}
	}
	return ret;
}

static int start_session(ssh_session session)
{
	struct SSHD_SESSION_DATA *session_data;
	if((session_data = (struct SSHD_SESSION_DATA *)malloc(sizeof(struct SSHD_SESSION_DATA))) != NULL) {
		memset(session_data, 0, sizeof(struct SSHD_SESSION_DATA));
		session_data->session = session;
		session_data->event = ssh_event_new();
		session_data->socket = -1;

		session_data->server_cb.userdata = session_data;
		session_data->server_cb.channel_open_request_session_function = channel_open;
		if(sshd_data.auth_mode == authModeKey) {
			session_data->server_cb.auth_pubkey_function = auth_publickey;
			ssh_set_auth_methods(session_data->session, SSH_AUTH_METHOD_PUBLICKEY);
		} else {
			session_data->server_cb.auth_password_function = auth_password,
			ssh_set_auth_methods(session_data->session, SSH_AUTH_METHOD_PASSWORD);
		}
		
		ssh_callbacks_init(&session_data->server_cb);
		ssh_set_server_callbacks(session_data->session, &session_data->server_cb);

		ssh_handle_key_exchange(session_data->session);
		ssh_event_add_session(session_data->event, session_data->session);

		session_data->mode = modeCheckAuth;

		if(sshd_data.session_data == NULL) {
			sshd_data.session_data = session_data;
		} else {
			struct SSHD_SESSION_DATA *data = sshd_data.session_data;
			while(data->next != NULL) {
				data = data->next;
			}
			data->next = session_data;
		}
		return 0;
	}
	return -1;
}

void close_ssh(struct COM_DATA *com)
{
	struct SSHD_SESSION_DATA *data = sshd_data.session_data;
	while(data != NULL) {
		if(data->com == com) {
			data->close_flag = 1;
			break;
		}
		data = data->next;
	}
}

static void clear_session(int all_flag)
{
	struct SSHD_SESSION_DATA *last_data = NULL;
	struct SSHD_SESSION_DATA *data = sshd_data.session_data;
	while(data != NULL) {
		if(data->clear_flag || all_flag) {
			struct SSHD_SESSION_DATA *next_data = data->next;
			if(check_debug()) {
				printf("close %d\n", data->socket);
			}
			if(last_data == NULL) {
				sshd_data.session_data = data->next;
			} else {
				last_data->next = data->next;
			}
			if(data->channel != NULL) {
				if(ssh_channel_is_open(data->channel)) {
					ssh_channel_send_eof(data->channel);
					ssh_channel_close(data->channel);
				}
			}
			if(data->event != NULL) {
				ssh_event_free(data->event);
			}
			if(data->session != NULL) {
				ssh_disconnect(data->session); 
				ssh_free(data->session);
			}
			close_connect_com(data->com);
			if(data->enter_id) {
				free(data->enter_id);
				data->enter_id = NULL;
			}
			if(data->enter_pass) {
				free(data->enter_pass);
				data->enter_pass = NULL;
			}
			free(data);
			data = next_data;
		} else {
			last_data = data;
			data = data->next;
		}
	}
}

static void check_ssh_listen()
{
	struct timeval timeout;
	fd_set mask;
	int width, ret;

	FD_ZERO(&mask);
	FD_SET(sshd_data.listen_socket, &mask);
	width = sshd_data.listen_socket + 1;
	timeout.tv_sec = 0;
	timeout.tv_usec = 100;
	ret = select(width, (fd_set *)&mask, NULL, NULL, &timeout);
	if(ret == -1) {
		return;
	} else if(ret != 0) {
		if(FD_ISSET(sshd_data.listen_socket, &mask)) {
			ssh_session session = ssh_new(); 
			if(ssh_bind_accept(sshd_data.bind, session) == SSH_OK) {
				if(start_session(session)) {
					ssh_disconnect(session); 
					ssh_free(session);
				}
			}
		}
	}
}

void check_ssh()
{
	if(sshd_data.active_flag) {
		struct SSHD_SESSION_DATA *session_data;

		check_ssh_listen();

		session_data = sshd_data.session_data;
		while(session_data != NULL) {
			ssh_event_dopoll(session_data->event, 10);
			if(session_data->mode == modeCheckAuth) {
				if(session_data->auth_flag != 0 && session_data->channel != NULL) {
					char *change_code = get_teraterm_change_code_string();
					session_data->socket = ssh_get_fd(session_data->session);
					if(check_debug()) {
						printf("connect %d\n", session_data->socket);
					}
					if((session_data->com = get_empty_com()) != NULL) {
						session_data->com->ssh_flag = 1;
						session_data->com->ssh_auth_mode = sshd_data.auth_mode;
						session_data->com->channel = session_data->channel;
						session_data->com->socket = session_data->socket;
						session_data->channel_cb.userdata = session_data;
						session_data->channel_cb.channel_data_function = data_function;
						session_data->channel_cb.channel_pty_request_function = pty_request;
						session_data->channel_cb.channel_shell_request_function = shell_request;
						ssh_callbacks_init(&session_data->channel_cb);
						ssh_set_channel_callbacks(session_data->channel, &session_data->channel_cb);
						session_data->mode = modeConnected;
						if(session_data->com->modem) {
							if(session_data->com->ring == ringNone) {
								set_dtr(session_data->com, TRUE);
								if(session_data->com->connect != connectNone) {
									send_connect(session_data->com);
								}
								start_connect(session_data->com);
							} else {
								session_data->com->status = statusRing;
								send_com(session_data->com, "RING\r\n");
								session_data->com->ring_count = 0;
								gettimeofday(&session_data->com->check_start, NULL);
							}
						}
						if(change_code) {
							ssh_channel_write(session_data->channel, change_code, strlen(change_code));
						}
					} else {
						char *full_string;
						if((full_string = get_full_string()) != NULL) {
							if(change_code) {
								ssh_channel_write(session_data->channel, change_code, strlen(change_code));
							}
							ssh_channel_write(session_data->channel, full_string, strlen(full_string));
						}
						session_data->mode = modeDisconnect;
					}
				} else {
					if(ssh_get_status(session_data->session) & (SSH_CLOSED | SSH_CLOSED_ERROR) || session_data->close_flag) {
						session_data->clear_flag = 1;
					}
				}
			} else if(session_data->mode == modeConnected) {
				if(!ssh_channel_is_open(session_data->channel) || !sshd_data.active_flag || session_data->close_flag) {
					int no;

					ssh_channel_send_eof(session_data->channel);
					ssh_channel_close(session_data->channel);
					session_data->channel = NULL;

					for(no = 0; no < 50 && (ssh_get_status(session_data->session) & (SSH_CLOSED | SSH_CLOSED_ERROR)) == 0 ; no++) {
						ssh_event_dopoll(session_data->event, 10); 
					}
					session_data->clear_flag = 1;
				}
			} else if(session_data->mode == modeDisconnect) {
				session_data->clear_flag = 1;
			}
			session_data = session_data->next;
		}
		clear_session(0);
	}
}

char *get_ssh_enter_id(struct COM_DATA *com)
{
	struct SSHD_SESSION_DATA *data = sshd_data.session_data;
	while(data != NULL) {
		if(data->com == com) {
			return data->enter_id;
		}
		data = data->next;
	}
	return "";
}

char *get_ssh_enter_pass(struct COM_DATA *com)
{
	struct SSHD_SESSION_DATA *data = sshd_data.session_data;
	while(data != NULL) {
		if(data->com == com) {
			return data->enter_pass;
		}
		data = data->next;
	}
	return "";
}

void send_ssh(struct COM_DATA *com, char *buffer, int length)
{
	if(com->channel != NULL) {
		ssh_channel_write(com->channel, buffer, length);
	}
}

int init_ssh(int port, char *host_key, int auth_mode, char *user, char *pass, char *pub_key)
{ 
	int ret;
	ssh_key key;

#ifdef ENABLE_SSHD_LOG
	ssh_set_log_callback(log_function);
	ssh_set_log_level(5);
#endif
	ret = ssh_pki_import_privkey_file(host_key, NULL, NULL, NULL, &key);
	ssh_key_free(key);
	if(ret != SSH_OK) {
		fprintf(stderr, "SSH: host key read error. %s\n", host_key);
		return -2;
	}
	sshd_data.auth_mode = auth_mode;
	sshd_data.user = user;
	sshd_data.pass = pass;
	sshd_data.pub_key = pub_key;

	ssh_init(); 
	sshd_data.bind = ssh_bind_new();

	ssh_bind_options_set(sshd_data.bind, SSH_BIND_OPTIONS_BINDPORT, &port);
	ssh_bind_options_set(sshd_data.bind, SSH_BIND_OPTIONS_RSAKEY, host_key);

	if(ssh_bind_listen(sshd_data.bind) == SSH_OK) {
		sshd_data.active_flag = 1;
		sshd_data.listen_socket = ssh_bind_get_fd(sshd_data.bind);
		return 0;
	}
	fprintf(stderr, "SSH: port %d listen error.\n", port);
	return -1;
}

void done_ssh()
{
	clear_session(1);
	if(sshd_data.bind != NULL) {
		int s = ssh_bind_get_fd(sshd_data.bind);
		if(s != -1) {
			close(s);
		}
		ssh_bind_free(sshd_data.bind);
	}
}

