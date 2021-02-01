int init_ssh(int port, char *host_key, int auth_mode, char *user, char *pass, char *pub_key);
void done_ssh();
void check_ssh();
void close_ssh(struct COM_DATA *com);
char *get_ssh_enter_id(struct COM_DATA *com);
char *get_ssh_enter_pass(struct COM_DATA *com);
char *create_password_hash(char *pass);
void send_ssh(struct COM_DATA *com, char *buffer, int length);

enum {
	authModePass,
	authModeKey,
	authModeBBS
};

