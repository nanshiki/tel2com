#define	TRUE			1
#define	FALSE			0

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
	int dsr;

	int socket;
	int ff_flag;
	int check_pos[2];
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

#define	CUT_STRING_COUNT	2

struct BASE_DATA {
	char *telnet_port;
	int telnet_command;
	int listen_socket;
	int no_comm_timer;
	int limit_timer;
	int keep_timer;
	char *cut_string[CUT_STRING_COUNT];
	char *after_string;
	char *full_string;
	int code;
	struct COM_DATA *com;
	struct SHUTDOWN_DATA *shutdown_data;
	int debug_flag;
};

