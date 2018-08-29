/*
    tel2com

    Copyright (c) 2018 Nanshiki Corporation

    This software is released under the MIT License.
    https://opensource.org/licenses/mit-license.php
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <termios.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#include <iconv.h>
#include <sys/time.h>

#include "t2c_def.h"
#include "uart.h"
#include "telnet.h"

#define	INI_FILE			"tel2com.ini"

#define	INITIAL_INTERVAL	2000
#ifdef PC_LINUX
#define	OPEN_COM_INTERVAL	3000
#endif

enum {
	sectionNone,
	sectionCom,
	sectionTelnet,
	sectionTime,
	sectionText,
	sectionDebug,
};

static const char *connect_list[] = {
	"none",
	"ata",
	"ring",
	NULL
};

static const char *parity_list[] = {
	"none",
	"odd",
	"even",
	NULL
};

static const char *on_off_list[] = {
	"off",
	"on",
	NULL
};

static const char *flow_list[] = {
	"none",
	"hard",
	NULL
};

#ifdef USE_GPIO
static const char *level_list[] = {
	"low",
	"high",
	NULL
};
#endif

static const char *code_list[] = {
	"utf8",
	"shiftjis",
	"euc",
	NULL
};

static struct BASE_DATA *base;

static void done()
{
	int no;

	if(base != NULL) {
		struct SHUTDOWN_DATA *data = base->shutdown_data;
		while(data != NULL) {
			struct SHUTDOWN_DATA *next_data = data->next;
			close(data->socket);
			free(data);
			data = next_data;
		}
		if(base->listen_socket != -1) {
			close(base->listen_socket);
		}
		if(base->telnet_port != NULL) {
			free(base->telnet_port);
		}
		for(no = 0 ; no < CUT_STRING_COUNT ; no++) {
			if(base->cut_string[no] != NULL) {
				free(base->cut_string[no]);
			}
		}
		if(base->after_string != NULL) {
			free(base->after_string);
		}
		if(base->full_string != NULL) {
			free(base->full_string);
		}
		if(base->com != NULL) {
			if(base->com->handle != -1) {
				set_dtr(base->com, FALSE);
				close_com(base->com);
			}
			if(base->com->socket != -1) {
				close(base->com->socket);
			}
			if(base->com->name != NULL) {
				free(base->com->name);
			}
			if(base->com->path != NULL) {
				free(base->com->path);
			}
			free(base->com);
		}
		free(base);
		base = NULL;
	}
}

static void sigcatch()
{
	done();
	exit(1);
}

char *get_after_string()
{
	return base->after_string;
}

char *get_full_string()
{
	return base->full_string;
}

struct COM_DATA *get_empty_com()
{
	if(base->com->socket == -1 && base->com->status == statusDisconnect) {
		return base->com;
	}
	return NULL;
}

int get_keep_timer()
{
	return base->keep_timer;
}

int check_telnet_command()
{
	return base->telnet_command;
}

int check_debug()
{
	return base->debug_flag;
}

int check_cut_string(struct COM_DATA *com, char ch)
{
	int no;

	for(no = 0 ; no < CUT_STRING_COUNT ; no++) {
		if(base->cut_string[no] != NULL) {
			if(base->cut_string[no][com->check_pos[no]] == ch) {
				com->check_pos[no]++;
				if(base->cut_string[no][com->check_pos[no]] == '\0') {
					return TRUE;
				}
			} else {
				com->check_pos[no] = 0;
			}
		}
	}
	return FALSE;
}

static void check_shutdown_socket()
{
	struct SHUTDOWN_DATA *last_data = NULL;
	struct SHUTDOWN_DATA *data = base->shutdown_data;
	while(data != NULL) {
		if(check_close(data->socket)) {
			struct SHUTDOWN_DATA *next_data = data->next;
			close(data->socket);
			if(check_debug()) {
				printf("close %d\n", data->socket);
			}
			if(last_data == NULL) {
				base->shutdown_data = data->next;
			} else {
				last_data->next = data->next;
			}
			free(data);
			data = next_data;
		} else {
			last_data = data;
			data = data->next;
		}
	}
}

void add_shutdown_socket(int socket)
{
	struct SHUTDOWN_DATA *new_data;
	if((new_data = (struct SHUTDOWN_DATA *)malloc(sizeof(struct SHUTDOWN_DATA))) != NULL) {
		new_data->next = NULL;
		new_data->socket = socket;
		if(base->shutdown_data == NULL) {
			base->shutdown_data = new_data;
		} else {
			struct SHUTDOWN_DATA *data = base->shutdown_data;
			while(data->next != NULL) {
				data = data->next;
			}
			data->next = new_data;
		}
	}
}

static void get_dir(char *dir, const char *src)
{
	char *tail;

	if((tail = strrchr(src, '/')) != NULL) {
		while(*src != '\0' && src != tail) {
			*dir++ = *src++;
		}
	}
	*dir = '\0';
}

static int progress_timer(struct timeval start)
{
	struct timeval now, diff;

	gettimeofday(&now, NULL);
	timersub(&now, &start, &diff);

	return (int)(diff.tv_sec * 1000 + (double)diff.tv_usec / 1000.0);
}

static void check_timer(struct COM_DATA *com)
{
	if(com->status == statusInitial) {
		if(progress_timer(com->check_start) >= INITIAL_INTERVAL) {
			com->status = statusDisconnect;
		}
	} else if(com->status == statusRing) {
		if(progress_timer(com->check_start) >= com->ring_interval) {
			com->ring_count++;
			if(com->ring_count >= com->ring) {
				set_dtr(com, TRUE);
				if(com->connect == connectRing) {
					send_connect(com);
				}
				start_connect(com);
			} else {
				send_com(com, "RING\r\n");
				gettimeofday(&com->check_start, NULL);
			}
		}
	} else if(com->status == statusWaitConnect) {
		if(progress_timer(com->check_start) >= com->connect_interval) {
			start_connect(com);
			send_connect(com);
		}
	} else if(com->status == statusConnect) {
		if(base->limit_timer != 0) {
			if(progress_timer(com->limit_start) >= base->limit_timer * 1000) {
				close_socket(com);
			}
		}
		if(base->no_comm_timer != 0) {
			if(progress_timer(com->no_comm_start) >= base->no_comm_timer * 1000) {
				close_socket(com);
			}
		}
		if(com->modem && com->dsr_cut) {
			if(!get_dsr(com)) {
				if(check_debug()) {
					printf("DSR OFF\n");
				}
				close_socket(com);
			}
		}
	} else if(com->status == statusKeep) {
		if(progress_timer(com->check_start) >= base->keep_timer * 1000) {
			com->status = statusDisconnect;
		}
#ifdef PC_LINUX
	} else if(com->status == statusWaitOpenCom) {
		if(progress_timer(com->check_start) >= OPEN_COM_INTERVAL) {
			close(com->handle);
			init_com(com);
			if(base->keep_timer == 0) {
				com->status = statusDisconnect;
			} else {
				com->status = statusKeep;
				gettimeofday(&com->check_start, NULL);
			}
		}
#endif
	}
}

static int get_value(const char *list[], char *value)
{
	int no = 0;

	for(no = 0 ; list[no] != NULL; no++) {
		if(!strcasecmp(list[no], value)) {
			return no;
		}
	}
	return 0;
}

static char *change_crlf_string(char *src_string)
{
	char *dst_string = src_string;

	if(src_string != NULL) {
		int length = strlen(src_string);
		if((dst_string = malloc(length + 1)) != NULL) {
			int no = 0;
			while(*src_string != '\0' && no < length) {
				if(*src_string == '\\') {
					src_string++;
					if(*src_string == 'n') {
						*(dst_string + no) = '\n';
					} else if(*src_string == 'r') {
						*(dst_string + no) = '\r';
					} else if(*src_string == 't') {
						*(dst_string + no) = '\t';
					} else if(*src_string == '\\') {
						*(dst_string + no) = '\\';
					} else {
						*(dst_string + no) = '\\';
						no++;
						*(dst_string + no) = *src_string;
					}
				} else {
					*(dst_string + no) = *src_string;
				}
				src_string++;
				no++;
			}
			*(dst_string + no) = '\0';
		}
	}
	return dst_string;
}

static char *convert_string(char *src_string, int code)
{
	char *dst_string = src_string;

	if(src_string != NULL) {
		iconv_t ic;
		if((ic = iconv_open((code == codeShiftJIS) ? "SHIFT-JIS" : "EUC", "UTF-8")) != (iconv_t)-1) {
			size_t src_length = strlen(src_string);
			size_t dst_length = src_length * 6;

			if((dst_string = malloc(dst_length + 1)) != NULL) {
				char *src, *dst;

				src = src_string;
				dst = dst_string;
				iconv(ic, &src, &src_length, &dst, &dst_length);
				*dst = '\0';

				free(src_string);
			}

			iconv_close(ic);
		}
	}
	return dst_string;
}

static int read_ini(char *ini_file)
{
	if((base = (struct BASE_DATA *)malloc(sizeof(struct BASE_DATA))) != NULL) {
		FILE *fp;

		memset(base, 0, sizeof(struct BASE_DATA));
		if((fp = fopen(ini_file, "r")) != NULL) {
			int section = sectionNone;
			char *item, *value;
			char line[PATH_MAX];

			while(fgets(line, PATH_MAX, fp) != NULL) {
				if(line[0] == '[') {
					if((item = strtok(&line[1], "]\r\n")) != NULL) {
						if(!strcasecmp(item, "com")) {
							if((base->com = (struct COM_DATA *)malloc(sizeof(struct COM_DATA))) == NULL) {
								break;
							}
							memset(base->com, 0, sizeof(struct COM_DATA));
							base->com->handle = -1;
							base->com->socket = -1;
							section = sectionCom;
#ifdef USE_GPIO
							base->com->dsr_pin = -1;
							base->com->dtr_pin = -1;
#endif
						} else if(!strcasecmp(item, "telnet")) {
							section = sectionTelnet;
						} else if(!strcasecmp(item, "time")) {
							section = sectionTime;
						} else if(!strcasecmp(item, "text")) {
							section = sectionText;
						} else if(!strcasecmp(item, "debug")) {
							section = sectionDebug;
						}
					}
				} else if(isalpha(line[0])) {
					if((item = strtok(line, "=\r\n")) != NULL) {
						if((value = strtok(NULL, "=\r\n")) != NULL) {
							if(section == sectionCom) {
								if(!strcasecmp(item, "name")) {
									base->com->name = strdup(value);
								} else if(!strcasecmp(item, "speed")) {
									base->com->speed = atoi(value);
								} else if(!strcasecmp(item, "data")) {
									base->com->data = atoi(value);
								} else if(!strcasecmp(item, "stop")) {
									base->com->stop = atoi(value);
								} else if(!strcasecmp(item, "parity")) {
									base->com->parity = get_value(parity_list, value);
								} else if(!strcasecmp(item, "ring")) {
									base->com->ring = atoi(value);
								} else if(!strcasecmp(item, "ring_interval")) {
									base->com->ring_interval = atoi(value);
								} else if(!strcasecmp(item, "connect")) {
									base->com->connect = get_value(connect_list, value);
								} else if(!strcasecmp(item, "connect_interval")) {
									base->com->connect_interval = atoi(value);
								} else if(!strcasecmp(item, "dsr_cut")) {
									base->com->dsr_cut = get_value(on_off_list, value);
								} else if(!strcasecmp(item, "flow")) {
									base->com->flow = get_value(flow_list, value);
								} else if(!strcasecmp(item, "modem")) {
									base->com->modem = get_value(on_off_list, value);
#ifdef USE_GPIO
								} else if(!strcasecmp(item, "dtr_pin")) {
									base->com->dtr_pin = atoi(value);
								} else if(!strcasecmp(item, "dsr_pin")) {
									base->com->dsr_pin = atoi(value);
								} else if(!strcasecmp(item, "dtr_on")) {
									base->com->dtr_on = get_value(level_list, value);
								} else if(!strcasecmp(item, "dsr_on")) {
									base->com->dsr_on = get_value(level_list, value);
#endif
								}
							} else if(section == sectionTelnet) {
								if(!strcasecmp(item, "port")) {
									base->telnet_port = strdup(value);
								} else if(!strcasecmp(item, "command")) {
									base->telnet_command = get_value(on_off_list, value);
								}
							} else if(section == sectionTime) {
								if(!strcasecmp(item, "no_comm")) {
									base->no_comm_timer = atoi(value);
								} else if(!strcasecmp(item, "limit")) {
									base->limit_timer = atoi(value);
								} else if(!strcasecmp(item, "keep")) {
									base->keep_timer = atoi(value);
								}
							} else if(section == sectionText) {
								if(!strcasecmp(item, "cut1")) {
									base->cut_string[0] = change_crlf_string(value);
								} else if(!strcasecmp(item, "cut2")) {
									base->cut_string[1] = change_crlf_string(value);
								} else if(!strcasecmp(item, "after")) {
									base->after_string = change_crlf_string(value);
								} else if(!strcasecmp(item, "full")) {
									base->full_string = change_crlf_string(value);
								} else if(!strcasecmp(item, "code")) {
									base->code = get_value(code_list, value);
								}
							} else if(section == sectionDebug) {
								if(!strcasecmp(item, "debug")) {
									base->debug_flag = atoi(value);
								}
							}
						}
					}
				}
			}
			fclose(fp);

			if(base->com != NULL) {
				if(strchr(base->com->name, '/') == NULL) {
					char path[PATH_MAX];
					snprintf(path, PATH_MAX, "/dev/%s", base->com->name);
					base->com->path = strdup(path);
				} else {
					base->com->path = strdup(base->com->name);
				}
				if(base->code != codeUTF8) {
					int no;
					for(no = 0 ; no < CUT_STRING_COUNT ; no++) {
						base->cut_string[no] = convert_string(base->cut_string[no], base->code);
					}
					base->after_string = convert_string(base->after_string, base->code);
					base->full_string = convert_string(base->full_string, base->code);
				}
				return TRUE;
			} else {
				fprintf(stderr, "serial port setting error.\n");
			}
		} else {
			fprintf(stderr, "%s open error.\n", ini_file);
		}
	}
	return FALSE;
}

int main(int argc, char *argv[])
{
	char dir[PATH_MAX];
	char ini_file[PATH_MAX];

	get_dir(dir, argv[0]);
	if(argc > 1) {
		snprintf(ini_file, PATH_MAX, "%s/%s", dir, argv[1]);
	} else {
		snprintf(ini_file, PATH_MAX, "%s/%s", dir, INI_FILE);
	}

	if(read_ini(ini_file)) {
		if((base->listen_socket = init_socket(base->telnet_port)) != -1) {
			if(!init_com(base->com)) {
				signal(SIGINT, sigcatch);
				signal(SIGTERM, sigcatch);

				while(1) {
					loop_listen(base->listen_socket);

					if(base->com->socket != -1) {
						receive_socket(base->com);
					}
					if(base->com->handle != -1) {
						receive_com(base->com);
					} else {
						init_com(base->com);
					}
					check_timer(base->com);

					check_shutdown_socket();
					usleep(1000);
				}
			} else {
				fprintf(stderr, "%s open error.\n", base->com->path);
			}
		} else {
			fprintf(stderr, "port %s listen error.\n", base->telnet_port);
		}
	}
	done();

	return 0;
}

