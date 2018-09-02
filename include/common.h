#ifndef _common_h_
#define _common_h_
//system header
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <shadow.h>
#include <linux/limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <linux/capability.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <semaphore.h>

//C++ header
#include <unordered_map>
#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ctime>
#include <cstdarg>
#include <list>

//macro
#define HERR_EXIT(x) \
	do{ \
		herror(x); \
		exit(0);\
	}while(0)

#define PERR_EXIT(x) \
        do{ \
                perror(x); \
                exit(0);\
        }while(0)

#define INFOF(name, fmt, ...)	\
	output(__FILE__, __LINE__, name, "INFOF", fmt, ##__VA_ARGS__)
#define WARN(name, fmt, ...)	\
	output(__FILE__, __LINE__, name, "WARN", fmt, ##__VA_ARGS__)
#define ERROR(name, fmt, ...)	\
	output(__FILE__, __LINE__, name, "ERROR", fmt, ##__VA_ARGS__)

#define MAX_COMMAND_LINE 1024
#define MAX_COMMAND 32
#define MAX_ARG 1024
//配置文件
#define IFTP_CONFIG_PATH "iftp.conf"


//other

#endif
