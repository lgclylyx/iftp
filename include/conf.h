#ifndef _conf_h_
#define _conf_h_

extern bool pasv_enable;
extern bool port_enable;
extern int listen_port;
extern int max_clients;
extern int max_per_ip;
extern int accept_timeout;
extern int connect_timeout;
extern int session_timeout;
extern int data_connection_timeout;
extern int local_umask;
extern int upload_max_rate;
extern int download_max_rate;
extern const char* listen_address;


void parseconf_load_file(const char* path);
void parseconf_load_setting(const char* setting);

#endif