#ifndef _inet_util_h_
#define _inet_util_h_

bool getlocalip(char *ip, int localfd);
int tcp_server(std::string ip_addr,unsigned int port);
int tcp_client(unsigned short port, int localfd);
int connect_timeout_f(int fd, struct sockaddr_in* addr,unsigned int wait_seconds);
int accept_timeout_f(int fd, unsigned int wait_seconds);
void send_fd(int sock_fd, int fd);
int recv_fd(int fd);
int setNoblock(int fd, int param);

#endif