#include <libssh/callbacks.h>
#include <libssh/server.h>

#ifndef TRUE
#define	TRUE			1
#endif
#ifndef FALSE
#define	FALSE			0
#endif

#define	TELNET_PORT		23
#define	RECEIVE_BUFFER_LENGTH	1200
#define	SEND_BUFFER_LENGTH		2500

enum {
	ringNone,
	ring1,
	ring2,
	ring3,
};

enum {
	connectNone,
	connectATA,
	connectRing,
};

enum {
	statusDisconnect,
	statusRing,
	statusWaitConnect,
	statusConnect,
	statusKeep,
	statusInitial,
#ifdef PC_LINUX
	statusWaitOpenCom,
#endif
};


enum {
	parityNone,
	parityOdd,
	parityEven,
};

enum {
	flowNone,
	flowHard,
};

enum {
	codeUTF8,
	codeShiftJIS,
	codeEUC,
};

enum {
	checkCut1,
	checkCut2,
	checkBbsId,
	checkBbsPassword,

	checkMax
};

struct SHUTDOWN_DATA {
	struct SHUTDOWN_DATA *next;
	int socket;
};

struct COM_DATA {
	int status;

	int handle;
	char *name;
	char *path;
	int speed;
	int data;
	int stop;
	int parity;
	int flow;
	struct termios old_tio;
	int ring;
	int ring_count;
	int ring_interval;
	int connect_interval;
	int connect;
	int modem;
	int dsr_cut;
	int ssh_flag;
	int ssh_auth_mode;
	int send_id_flag;
	int send_pass_flag;

	int socket;
	ssh_channel channel;
	int ff_flag;
	int check_pos[checkMax];
	int check_flag;
	int line_length;
	char line_buffer[RECEIVE_BUFFER_LENGTH];
	struct timeval check_start;
	struct timeval no_comm_start;
	struct timeval limit_start;
#ifdef USE_GPIO
	int dsr_pin;
	int dsr_on;
	int dtr_pin;
	int dtr_on;
#endif
#ifdef SYS_CLASS_GPIO
	int dsr_fd;
	int dtr_fd;
#endif
};

struct BASE_DATA {
	char *telnet_port;
	int telnet_command;
	int listen_socket;
	int no_comm_timer;
	int limit_timer;
	int keep_timer;
	char *check_string[checkMax];
	char *after_string;
	char *full_string;
	int code;
	struct COM_DATA *com;
	struct SHUTDOWN_DATA *shutdown_data;
	int debug_flag;

	int ssh_flag;
	int ssh_port;
	int ssh_auth_mode;
	char *ssh_id_string;
	char *ssh_pass_string;
	char *ssh_host_key;
	char *ssh_pub_key;
	int teraterm_change_code;
	char *teraterm_change_code_string;
};

