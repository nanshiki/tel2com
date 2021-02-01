int check_close(int socket);
int init_socket(char *port);
void close_socket(struct COM_DATA *com_data);
void receive_socket(struct COM_DATA *com_data);
void start_connect(struct COM_DATA *com_data);
void loop_listen(int listen_socket);
void close_connect_com(struct COM_DATA *com);
