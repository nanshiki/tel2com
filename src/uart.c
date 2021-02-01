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
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <sys/ioctl.h>
#include <linux/serial.h>

#ifdef WIRING_PI
#include <wiringPi.h>
#endif

#include "t2c_def.h"
#include "tel2com.h"
#include "telnet.h"
#include "sshd.h"
#include "uart.h"

#ifdef SYS_CLASS_GPIO
static int set_gpio(int pin, int out_flag)
{
	int fd;
	char buff[PATH_MAX];

	if((fd = open("/sys/class/gpio/export", O_WRONLY)) == -1) {
		return -1;
	}
	snprintf(buff, PATH_MAX, "%d", pin);
	write(fd, buff, strlen(buff));
	close(fd);	

	snprintf(buff, PATH_MAX, "/sys/class/gpio/gpio%d/direction", pin);
	if((fd = open(buff, O_WRONLY)) == -1) {
		return -1;
	}
	if(out_flag) {
		strcpy(buff, "out");
	} else {
		strcpy(buff, "in");
	}
	write(fd, buff, strlen(buff));
	close(fd);

	snprintf(buff, PATH_MAX, "/sys/class/gpio/gpio%d/value", pin);
	return open(buff, out_flag ? O_WRONLY : O_RDONLY);
}
#endif

int init_com(struct COM_DATA *com)
{
	if((com->handle = open(com->path, O_RDWR | O_NOCTTY | O_NONBLOCK)) >= 0) {
		struct termios tio = { 0 };

		tio.c_cflag = CLOCAL | CREAD;
		if(com->data == 7) {
			tio.c_cflag |= CS7;
		} else {
			tio.c_cflag |= CS8;
		}
		if(com->stop == 2) {
			tio.c_cflag |= CSTOPB;
		}
		if(com->flow == flowHard) {
			tio.c_cflag |= CRTSCTS;
		}
		switch(com->speed) {
		case 9600:
			tio.c_cflag |= B9600;
			break;
		case 19200:
			tio.c_cflag |= B19200;
			break;
		case 38400:
			tio.c_cflag |= B38400;
			break;
		case 57600:
			tio.c_cflag |= B57600;
			break;
		case 115200:
			tio.c_cflag |= B115200;
			break;
#ifdef B230400
    	case 230400:
    		tio.c_cflag |= B230400;
    		break;
#endif
#ifdef B460800
    	case 460800:
    		tio.c_cflag |= B460800;
    		break;
#endif
#ifdef B500000
    	case 500000:
    		tio.c_cflag |= B500000;
    		break;
#endif
#ifdef B576000
		case 576000:
			tio.c_cflag |= B576000;
			break;
#endif
#ifdef B921600
    	case 921600:
    		tio.c_cflag |= B921600;
    		break;
#endif
#ifdef B1000000
		case 1000000:
			tio.c_cflag |= B1000000;
			break;
#endif
#ifdef B1152000
		case 1152000:
			tio.c_cflag |= B1152000;
			break;
#endif
#ifdef B1500000
		case 1500000:
			tio.c_cflag |= B1500000;
			break;
#endif
#ifdef B2000000
		case 2000000:
			tio.c_cflag |= B2000000;
			break;
#endif
#ifdef B2500000
    	case 2500000:
    		tio.c_cflag |= B2500000;
    		break;
#endif
#ifdef B3000000
    	case 3000000:
    		tio.c_cflag |= B3000000;
    		break;
#endif
#ifdef B3500000
    	case 3500000:
    		tio.c_cflag |= B3500000;
    		break;
#endif
#ifdef B4000000
		case 4000000:
			tio.c_cflag |= B4000000;
			break;
		}
#endif
		if(com->parity == parityEven) {
			tio.c_cflag |= PARENB;
		} else if(com->parity == parityOdd) {
			tio.c_cflag |= PARENB | PARODD;
		}
		tio.c_iflag = IGNPAR;
		tio.c_oflag = 0;
		tio.c_lflag = 0;

		tcgetattr(com->handle, &com->old_tio);
		tcflush(com->handle, TCIFLUSH);

		if(tcsetattr(com->handle, TCSANOW, &tio) != 0) {
			close(com->handle);
			com->handle = -1;
			return -1;
		}
#ifdef WIRING_PI
		wiringPiSetup();
		if(com->dsr_pin != -1) {
			pinMode(com->dsr_pin, INPUT);
		}
		if(com->dtr_pin != -1) {
			pinMode(com->dtr_pin, OUTPUT);
			set_dtr(com, FALSE);
		}
#endif
#ifdef SYS_CLASS_GPIO
		com->dsr_fd = -1;
		if(com->dsr_pin != -1) {
			com->dsr_fd = set_gpio(com->dsr_pin, FALSE);
		}
		com->dtr_fd = -1;
		if(com->dtr_pin != -1) {
			com->dtr_fd = set_gpio(com->dtr_pin, TRUE);
		}
#endif
		com->line_length = 0;
		return 0;
	}
	return -1;
}

void set_dtr(struct COM_DATA *com, int flag)
{
	if(com->handle != -1) {
#ifdef PC_LINUX
		int data;

		ioctl(com->handle, TIOCMGET, &data);
		if(flag) {
			data |= TIOCM_DTR;
		} else {
			data &= !TIOCM_DTR;
		}
		ioctl(com->handle, TIOCMSET, &data);
#endif
#ifdef WIRING_PI
		if(com->dtr_pin != -1) {
			if(flag) {
				if(com->dtr_on) {
					digitalWrite(com->dtr_pin, HIGH);
				} else {
					digitalWrite(com->dtr_pin, LOW);
				}
			} else {
				if(com->dtr_on) {
					digitalWrite(com->dtr_pin, LOW);
				} else {
					digitalWrite(com->dtr_pin, HIGH);
				}
			}
		} else {
			int data;

			ioctl(com->handle, TIOCMGET, &data);
			if(flag) {
				data |= TIOCM_DTR;
			} else {
				data &= !TIOCM_DTR;
			}
			ioctl(com->handle, TIOCMSET, &data);
		}
#endif
#ifdef SYS_CLASS_GPIO
		if(com->dtr_fd != -1) {
			if(flag) {
				if(com->dtr_on) {
					write(com->dtr_fd, "1", 1);
				} else {
					write(com->dtr_fd, "0", 1);
				}
			} else {
				if(com->dtr_on) {
					write(com->dtr_fd, "0", 1);
				} else {
					write(com->dtr_fd, "1", 1);
				}
			}
			lseek(com->dtr_fd, 0, SEEK_SET);
		} else {
			int data;

			ioctl(com->handle, TIOCMGET, &data);
			if(flag) {
				data |= TIOCM_DTR;
			} else {
				data &= !TIOCM_DTR;
			}
			ioctl(com->handle, TIOCMSET, &data);
		}
#endif
		if(check_debug()) {
			printf("DTR %s\n", flag ? "ON" : "OFF");
		}
	}
}

int get_dsr(struct COM_DATA *com)
{
	if(com->handle != -1) {
#ifdef PC_LINUX
		int data;

		ioctl(com->handle, TIOCMGET, &data);
		if(data & TIOCM_DSR) {
			return TRUE;
		}
#endif
#ifdef WIRING_PI
		if(com->dsr_pin != -1) {
			if(digitalRead(com->dsr_pin) == HIGH) {
				if(com->dsr_on) {
					return TRUE;
				}
			} else {
				if(!com->dsr_on) {
					return TRUE;
				}
			}
		} else {
			int data;

			ioctl(com->handle, TIOCMGET, &data);
			if(data & TIOCM_DSR) {
				return TRUE;
			}
		}
#endif
#ifdef SYS_CLASS_GPIO
		if(com->dsr_fd != -1) {
			char buff[PATH_MAX];

			read(com->dsr_fd, buff, PATH_MAX);
			lseek(com->dsr_fd, 0, SEEK_SET);
			if(atoi(buff) != 0) {
				if(com->dsr_on) {
					return TRUE;
				}
			} else {
				if(!com->dsr_on) {
					return TRUE;
				}
			}
		} else {
			int data;

			ioctl(com->handle, TIOCMGET, &data);
			if(data & TIOCM_DSR) {
				return TRUE;
			}
		}
#endif
	}
	return FALSE;
}

void send_connect(struct COM_DATA *com)
{
	char buff[PATH_MAX];
	snprintf(buff, PATH_MAX, "CONNECT %d\r\n", com->speed);
	send_com(com, buff);
}

void send_com(struct COM_DATA *com, char *text)
{
	if(com->handle != -1 && text != NULL) {
		write(com->handle, text, strlen(text));

		if(check_debug()) {
			printf("S:%s", text);
		}
	}
}

void close_com(struct COM_DATA *com)
{
	tcsetattr(com->handle, TCSANOW, &com->old_tio);
	close(com->handle);
	com->handle = -1;
	if(com->ssh_flag) {
		close_ssh(com);
	} else {
		if(com->socket != -1) {
			close_socket(com);
		}
	}
}

void receive_com(struct COM_DATA *com)
{
	char receive_buffer[RECEIVE_BUFFER_LENGTH];
	int receive_length = read(com->handle, receive_buffer, RECEIVE_BUFFER_LENGTH);
	if(receive_length == -1) {
		close_com(com);
		return;
	} else if(receive_length > 0) {
		if(com->status != statusConnect) {
			if(com->modem) {
				int no;
				for(no = 0 ; no < receive_length ; no++) {
					if(receive_buffer[no] == 0x0d || com->line_length >= RECEIVE_BUFFER_LENGTH - 1) {
						com->line_buffer[com->line_length] = '\0';
						if(check_debug()) {
							printf("R:%s\n", com->line_buffer);
						}
						if(com->line_buffer[0] == 'A' && com->line_buffer[1] == 'T') {
							if(com->line_buffer[2] == 'A') {
								if(com->socket != -1) {
									if(com->connect == connectATA) {
										if(com->connect_interval == 0) {
											set_dtr(com, TRUE);
											send_connect(com);
										} else {
											com->status = statusWaitConnect;
											gettimeofday(&com->check_start, NULL);
										}
									}
								}
								if(com->status != statusWaitConnect) {
									start_connect(com);
								}
							} else {
								send_com(com, "OK\r\n");
								com->status = statusInitial;
								gettimeofday(&com->check_start, NULL);
							}
						}
						com->line_length = 0;
					} else if(receive_buffer[no] != 0x0a) {
						com->line_buffer[com->line_length++] = receive_buffer[no];
					}
				}
			}
		} else {
			int no, check_no;
			if(com->ssh_flag) {
				send_ssh(com, receive_buffer, receive_length);
			} else {
				if(check_telnet_command()) {
					char send_buffer[SEND_BUFFER_LENGTH];
					int send_length = 0;
					for(no = 0 ; no < receive_length ; no++) {
						if(receive_buffer[no] == (char)0xff) {
							send_buffer[send_length++] = 0xff;
						}
						send_buffer[send_length++] = receive_buffer[no];
					}
					send(com->socket, send_buffer, send_length, 0);
				} else {
					send(com->socket, receive_buffer, receive_length, 0);
				}
			}

			gettimeofday(&com->no_comm_start, NULL);

			for(no = 0 ; no < receive_length ; no++) {
				if((check_no = check_string(com, receive_buffer[no])) != checkMax) {
					if(check_no == checkCut1 || check_no == checkCut2) {
						com->check_flag = TRUE;
						if(com->ssh_flag) {
							close_ssh(com);
						} else {
							close_socket(com);
						}
					} else if(com->ssh_flag && com->ssh_auth_mode == authModeBBS) {
						if(check_no == checkBbsId && com->send_id_flag == 0) {
							com->send_id_flag = 1;
							send_com(com, get_ssh_enter_id(com));
							send_com(com, "\r");
						} else if(check_no == checkBbsPassword && com->send_pass_flag == 0) {
							com->send_pass_flag = 1;
							send_com(com, get_ssh_enter_pass(com));
							send_com(com, "\r");
						}
					}
				}
			}
		}
	}
}
