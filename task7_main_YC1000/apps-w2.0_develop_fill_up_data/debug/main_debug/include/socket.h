int createsocket(void);
int connect_socket(int fd_sock);
int send_record(int fd_sock,char *sendbuff);
void close_socket(int fd_sock);
void get_ip(char ip_buff[16]);
