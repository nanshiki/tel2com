/*
    tel2com

    Copyright (c) 2018 Nanshiki Corporation

    This software is released under the MIT License.
    https://opensource.org/licenses/mit-license.php
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include "t2c_def.h"
#include "tel2com.h"
#include "telnet.h"
#include "uart.h"

static int set_block(int fd, int flag)
{
	int data;

	if((data = fcntl(fd, F_GETFL, 0)) == -1) {
		return -1;
	}
	if(flag) {
		fcntl(fd, F_SETFL, data | O_NONBLOCK);
	} else {
		fcntl(fd, F_SETFL, data & ~O_NONBLOCK);
	}

	return 0;
}

int check_close(int sock)
{
	fd_set mask;
	struct timeval timeout;
	int width, ret;

	FD_ZERO(&mask);
	FD_SET(sock, &mask);
	width = sock + 1;

	timeout.tv_sec = 0;
	timeout.tv_usec = 100;
	if((ret = select(width, (fd_set *)&mask, NULL, NULL, &timeout)) > 0) {
		if(FD_ISSET(sock, &mask)) {
			char buffer[RECEIVE_BUFFER_LENGTH];
			if(recv(sock, buffer, RECEIVE_BUFFER_LENGTH, 0) <= 0) {
				return TRUE;
			}
		}
	} else if(ret == -1) {
		return TRUE;
	}
	return FALSE;
}

int init_socket(char *port)
{
	struct addrinfo info, *res;
	int listen_socket;

	memset(&info, 0, sizeof(info));
	info.ai_family = AF_INET;
	info.ai_socktype = SOCK_STREAM;
	info.ai_flags = AI_PASSIVE;
	if(getaddrinfo(NULL, port, &info, &res) == 0) {
		char host[NI_MAXHOST], serv[NI_MAXSERV];
		if(getnameinfo(res->ai_addr, res->ai_addrlen, host, sizeof(host), serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
			if((listen_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) != -1) {
				int opt = 1;
				if(setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0) {
					if(bind(listen_socket, res->ai_addr, res->ai_addrlen) == 0) {
						if(listen(listen_socket, SOMAXCONN) == 0) {
							freeaddrinfo(res);
							return listen_socket;
						}
					}
				}
				close(listen_socket);
			}
		}
		freeaddrinfo(res);
	}
	return -1;
}

void close_connect_com(struct COM_DATA *com)
{
	if(com != NULL) {
		com->socket = -1;
		com->channel = NULL;
		if(!com->check_flag) {
			char *after_string;
			if((after_string = get_after_string()) != NULL) {
				send_com(com, after_string);
			}
		}
		if(com->modem) {
			set_dtr(com, FALSE);
#ifdef PC_LINUX
			com->status = statusWaitOpenCom;
			gettimeofday(&com->check_start, NULL);
			return;
#endif
		}
		if(get_keep_timer() == 0) {
			com->status = statusDisconnect;
		} else {
			com->status = statusKeep;
			gettimeofday(&com->check_start, NULL);
		}
	}
}

void close_socket(struct COM_DATA *com)
{
	close(com->socket);
	if(check_debug()) {
		printf("close %d\n", com->socket);
	}
	close_connect_com(com);
}

void receive_socket(struct COM_DATA *com)
{
	fd_set mask;
	struct timeval timeout;
	int width, ret;

	FD_ZERO(&mask);
	FD_SET(com->socket, &mask);
	width = com->socket + 1;

	timeout.tv_sec = 0;
	timeout.tv_usec = 100;
	ret = select(width, (fd_set *)&mask, NULL, NULL, &timeout);
	if(ret == -1) {
		close_socket(com);
		return;
	} else if(ret != 0) {
		if(FD_ISSET(com->socket, &mask)) {
			char receive_buffer[RECEIVE_BUFFER_LENGTH];
			int receive_length;
			if((receive_length = recv(com->socket, receive_buffer, RECEIVE_BUFFER_LENGTH, 0)) > 0) {
				if(check_telnet_command()) {
					int no, send_length;
					char send_buffer[SEND_BUFFER_LENGTH];

					send_length = 0;
					for(no = 0 ; no < receive_length ; no++) {
						if(com->ff_flag > 0) {
							com->ff_flag--;
							if(com->ff_flag == 1 && (unsigned char)receive_buffer[no] == 0xff) {
								send_buffer[send_length++] = receive_buffer[no];
								com->ff_flag = 0;
							}
						} else {
							if((unsigned char)receive_buffer[no] == 0xff) {
								com->ff_flag = 2;
							} else {
								send_buffer[send_length++] = receive_buffer[no];
							}
						}
					}
					if(send_length > 0) {
						write(com->handle, send_buffer, send_length);
					}
				} else {
					com->send_id_flag = 1;
					com->send_pass_flag = 1;
					write(com->handle, receive_buffer, receive_length);
				}
				gettimeofday(&com->no_comm_start, NULL);
			} else {
				close_socket(com);
			}
		}
	}
}

void start_connect(struct COM_DATA *com)
{
	com->status = statusConnect;
	memset(com->check_pos, 0, sizeof(com->check_pos));
	com->send_id_flag = 0;
	com->send_pass_flag = 0;
	com->ff_flag = 0;
	gettimeofday(&com->limit_start, NULL);
	com->no_comm_start = com->limit_start;
}

void loop_listen(int listen_socket)
{
	struct sockaddr_storage from;
	struct timeval timeout;
	fd_set mask;
	int width, ret;
	int new_socket;
	socklen_t len;

	FD_ZERO(&mask);
	FD_SET(listen_socket, &mask);
	width = listen_socket + 1;
	timeout.tv_sec = 0;
	timeout.tv_usec = 100;
	ret = select(width, (fd_set *)&mask, NULL, NULL, &timeout);
	if(ret == -1) {
		return;
	} else if(ret != 0) {
		if(FD_ISSET(listen_socket, &mask)) {
			len = (socklen_t)sizeof(from);
			if((new_socket = accept(listen_socket, (struct sockaddr *)&from, &len)) != -1) {
				struct COM_DATA *com;
				char *change_code = get_teraterm_change_code_string();

				if(check_debug()) {
					printf("connect %d\n", new_socket);
				}
				if((com = get_empty_com()) != NULL) {

					com->socket = new_socket;
					set_block(com->socket, TRUE);

					if(check_telnet_command()) {
						send(com->socket, "\x0ff\x0fb\x000", 3, 0);
						send(com->socket, "\x0ff\x0fd\x000", 3, 0);
						send(com->socket, "\x0ff\x0fb\x001", 3, 0);
					}
					if(change_code != NULL) {
						send(com->socket, change_code, strlen(change_code), 0);
					}
					com->ff_flag = 0;
					if(com->modem) {
						if(com->ring == ringNone) {
							set_dtr(com, TRUE);
							if(com->connect != connectNone) {
								send_connect(com);
							}
							start_connect(com);
						} else {
							com->status = statusRing;
							send_com(com, "RING\r\n");
							com->ring_count = 0;
							gettimeofday(&com->check_start, NULL);
						}
					} else {
						start_connect(com);
					}
				} else {
					char *full_string;
					if((full_string = get_full_string()) != NULL) {
						if(change_code != NULL) {
							send(new_socket, change_code, strlen(change_code), 0);
						}
						send(new_socket, full_string, strlen(full_string), 0);
						shutdown(new_socket, SHUT_WR);
						add_shutdown_socket(new_socket);
					} else {
						close(new_socket);
						if(check_debug()) {
							printf("close %d\n", new_socket);
						}
					}
				}
			}
		}
	}
}

