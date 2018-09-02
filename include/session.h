#ifndef _session_h_
#define _session_h_

/**
 *cmdLine: 接收到的命令行
 *cmd: 指令
 *arg: 指令参数
 *ctrl_fd： 控制连接套接字
 *parent_fd： unused
 *child_fd: unused
 *data_fd: 数据连接套接字
 *pasv_fd: 被动连接监听套接字
 *uid: 用户ID
 *gid: 用户组ID
 *dir: 该会话当前所处的目录
 *is_ascii: 传输模式控制
 *port_addr: port模式下,客户端数据连接监听地址 
 */

struct session{
	char cmdLine[MAX_COMMAND_LINE];
	char cmd[MAX_COMMAND];
	char arg[MAX_ARG];
	int epollfd;
	int ctrl_fd;
	int parent_fd;
	int child_fd;
	int data_fd;
	int pasv_fd;

	// 用户信息
	uid_t uid;
	gid_t gid;
	char dir[PATH_MAX];
	// 传输模式，true：ascii; false: binary;
	bool is_ascii;
	// port模式地址
	struct sockaddr_in* port_addr;

	session(){
		is_ascii = false;
		port_addr = NULL;
		pasv_fd = -1;
		parent_fd = -1;
		child_fd = -1;
	}

	session(const session& sess) {
		copy(sess);
	}

	session& operator=(const session& sess) {
		if(NULL != port_addr) {
			free(port_addr);
		}
		copy(sess);
	}

	~session() {
		if(NULL != port_addr) {
			free(port_addr);
		}
	}
private:
	void copy(const session& sess) {
		memcpy(this, &sess, sizeof(session));
		if(NULL != port_addr) {
			port_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
			memcpy(port_addr, sess.port_addr, sizeof(struct sockaddr_in));
		}
	}
};

#endif