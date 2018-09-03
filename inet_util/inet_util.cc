#include "common.h"
#include "log.h"

using std::string;

/**
 * 这遇到了一个BUG，以前的策略是使用gethostname和gethostbyname来
 * 获取本地地址，并绑定到data_fd,用于在port模式下连接client，发送
 * 数据，但是这样获取到的ip是127.0.x.x。而不是对应client的端口的ip,
 * 所以改为传入ctrl_fd,通过getsockname来获取对应client端口的IP。
 */
bool getlocalip(char *ip, int localfd){
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);

	if(getsockname(localfd, (struct sockaddr*)&addr, &addrlen) < 0) {
		ERROR("iftp", "%s\n", "getlocalip: something wrong happend on getsockname.");
		return false;
	}

	if(NULL == inet_ntop(addr.sin_family, &addr.sin_addr, ip, 16)) {
		ERROR("iftp", "%s\n", "getlocalip: something wrong happend on inet_ntop.");
		return false;
	}

	return true;
}

int setNoblock(int fd, int param) {
	return ioctl(fd, FIONBIO, &param);
}

int connect_timeout_f(int fd, struct sockaddr_in* addr,unsigned int wait_seconds) {
	int ret;
	socklen_t addrlen = sizeof(struct sockaddr_in);

	if(wait_seconds > 0)
		setNoblock(fd,1);

	ret = connect(fd, (struct sockaddr*)addr, addrlen);
	if(ret < 0 && errno == EINPROGRESS) {
		fd_set connect_fdset;
		struct timeval timeout;
		FD_ZERO(&connect_fdset);
		FD_SET(fd,&connect_fdset);
		timeout.tv_sec = wait_seconds;
		timeout.tv_usec = 0;

		do {
			ret = select(fd+1,NULL,&connect_fdset,NULL,&timeout);
		} while(ret < 0 && errno == EINTR);

		if(ret == 0) {
			ERROR("iftp", "%s\n", "connect_timeout_f: connect to client timeout.");
			ret = -1;
			errno = ETIMEDOUT;
		} else if(ret < 0) {
			ERROR("iftp", "%s\n", "connect_timeout_f: something wrong happend on select.");
			return -1;
		} else if(ret == 1) {
			int err;
			socklen_t socklen = sizeof(err);

			int sockoptret = getsockopt(fd,SOL_SOCKET,SO_ERROR,&err,&socklen);
			if(sockoptret == -1) {
				ERROR("iftp", "%s\n", "connect_timeout_f: something wrong happend on getsockopt.");
				return -1;
			} if(err == 0)
				return 0;
			else{
				errno = err;
				ERROR("iftp", "%s%d.\n", "connect_timeout_f: something wrong, errno: ", err);
				ret = -1;
			}
		}
	}
	if(wait_seconds > 0) {
		setNoblock(fd, 0);
	}
	return ret;
}

int accept_timeout_f(int fd, unsigned int wait_seconds) {
	int ret;
	fd_set accept_fdset;
	struct timeval timeout;
	FD_ZERO(&accept_fdset);
	FD_SET(fd, &accept_fdset);
	timeout.tv_sec = wait_seconds;
	timeout.tv_usec = 0;

	if(wait_seconds > 0)
		setNoblock(fd, 1);

	do {
		ret = select(fd+1, &accept_fdset, NULL, NULL, &timeout);
	} while((ret < 0) && (errno == EINTR || errno == EWOULDBLOCK || errno == ECONNABORTED || errno == EPROTO));

	if(0 == ret) {
		ERROR("iftp", "%s\n", "accept_timeout: accept connection timeout.");
		ret = -1;
		errno = ETIMEDOUT;		
	} else if(ret < 0) {
		ERROR("iftp", "%s\n", "accept_timeout: something wrong happend on select.");
		ret = -1;
	} else if(1 == ret) {
		ret = accept(fd, NULL, NULL);
	}

	if(wait_seconds > 0) {
		setNoblock(fd, 0);
	}

	return ret;
}

int tcp_client(unsigned short port, int localfd){
	int sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd < 0)
		PERR_EXIT("socket");

	if(port > 0){
		int on = 1;
		if((setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(const char*)&on,sizeof(on))) < 0)
			PERR_EXIT("socket");
		struct sockaddr_in addr;
		socklen_t addr_len = sizeof(struct sockaddr_in);
		memset(&addr,0,addr_len);
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);

		char ip[16];
		getlocalip(ip, localfd);
		addr.sin_addr.s_addr = inet_addr(ip);

		if(-1 == bind(sockfd,(struct sockaddr*)&addr,addr_len))
			PERR_EXIT("bind");
	}
	return sockfd;
}

int tcp_server(string ip_addr,unsigned int port) {
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if(-1 == fd) {
		ERROR("iftp", "%s\n", "tcp_server: failed to create socket.");
		return -1;
	}

	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(struct sockaddr_in);
	memset(&addr, 0, addr_len);

	addr.sin_family = AF_INET;
	if(!ip_addr.empty()) {
		if(inet_aton(ip_addr.c_str(), &addr.sin_addr) == 0) {
			struct hostent *hp;
			hp = gethostbyname(ip_addr.c_str());
			if(NULL == hp) {
				ERROR("iftp", "%s\n", "tcp_server: failed to gethostbyname.");
				return -1;
			}
			addr.sin_addr = *((struct in_addr*)hp->h_addr);
		}
	} else
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);
	
	if(-1 == bind(fd, (struct sockaddr*)&addr, addr_len)) {
		ERROR("iftp", "%s\n", "tcp_server: failed to bind.");
		return -1;
	}
	if(-1 == listen(fd, 5)) {
		ERROR("iftp", "%s\n", "tcp_server: failed to listen.");
		return -1;
	}
	return fd;
}

static ssize_t read_fd(int fd, void *ptr, size_t nbytes, int *recvfd){
	struct msghdr	msg;
	struct iovec	iov[1];
	ssize_t			n;

	union {
	  struct cmsghdr	cm;
	  char				control[CMSG_SPACE(sizeof(int))];
	} control_un;
	struct cmsghdr	*cmptr;

	msg.msg_control = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);

	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	iov[0].iov_base = ptr;
	iov[0].iov_len = nbytes;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	if ( (n = recvmsg(fd, &msg, 0)) <= 0)
		return(n);

	if ( (cmptr = CMSG_FIRSTHDR(&msg)) != NULL &&
	    cmptr->cmsg_len == CMSG_LEN(sizeof(int))) {
		if (cmptr->cmsg_level != SOL_SOCKET)
			PERR_EXIT("control level != SOL_SOCKET");
		if (cmptr->cmsg_type != SCM_RIGHTS)
			PERR_EXIT("control type != SCM_RIGHTS");
		*recvfd = *((int *) CMSG_DATA(cmptr));
	} else
		*recvfd = -1;		/* descriptor was not passed */

	return(n);
}

static ssize_t
write_fd(int fd, void *ptr, size_t nbytes, int sendfd)
{
	struct msghdr	msg;
	struct iovec	iov[1];

	union {
	  struct cmsghdr	cm;
	  char				control[CMSG_SPACE(sizeof(int))];
	} control_un;
	struct cmsghdr	*cmptr;

	msg.msg_control = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);

	cmptr = CMSG_FIRSTHDR(&msg);
	cmptr->cmsg_len = CMSG_LEN(sizeof(int));
	cmptr->cmsg_level = SOL_SOCKET;
	cmptr->cmsg_type = SCM_RIGHTS;
	*((int *) CMSG_DATA(cmptr)) = sendfd;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	iov[0].iov_base = ptr;
	iov[0].iov_len = nbytes;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	return(sendmsg(fd, &msg, 0));
}

int recv_fd(int fd){
	int recvfd;
	char c;
	if (read_fd(fd, &c, 1, &recvfd) < 0)
			PERR_EXIT("recv_fd");

	return recvfd;
}

void send_fd(int sock_fd, int fd){
	char c = 'a';
	if (write_fd(sock_fd, &c, 1, fd) < 0)
		PERR_EXIT("send_fd");
}