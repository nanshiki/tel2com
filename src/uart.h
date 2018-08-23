void close_com(struct COM_DATA *com);
void receive_com(struct COM_DATA *com);
int init_com(struct COM_DATA *com_data);
void set_dtr(struct COM_DATA *com_data, int flag);
int get_dsr(struct COM_DATA *com_data);
void send_connect(struct COM_DATA *com_data);
void send_com(struct COM_DATA *com_data, char *text);
