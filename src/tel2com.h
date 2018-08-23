void add_shutdown_socket(int socket);
char *get_after_string();
char *get_full_string();
struct COM_DATA *get_empty_com();
int get_keep_timer();
int check_cut_string(struct COM_DATA *com, char ch);
int check_debug();
int check_telnet_command();
