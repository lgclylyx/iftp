#include "common.h"
#include "log.h"
#include "conf.h"
#include "inet_util.h"
#include "session.h"
#include "lock_map.h"
#include "worker.h"
#include "threadPool.h"
#include "RIO.h"
#include "strop.h"
#include "global.h"

static void do_user(session& sess);
static void do_pass(session& sess);
static void do_syst(session& sess);
static void do_feat(session& sess);
static void do_pwd(session& sess);
static void do_type(session& sess);
static void do_port(session& sess);
static void do_list(session& sess);
static void do_pasv(session& sess);
static void do_cwd(session& sess);

static struct ftpcmd_t{
	const char* cmd;
	void(*cmd_handler)(session& sess);
}ctrl_cmds[]={
	/*访问控制命令*/
	{"USER",&do_user},
	{"PASS",&do_pass},
	{"SYST",&do_syst},
	{"FEAT",&do_feat},
	{"PWD",&do_pwd},
	{"TYPE",&do_type},
	{"PORT",&do_port},
	{"LIST",&do_list},
	{"PASV",&do_pasv},
	{"CWD",&do_cwd}
};

worker::worker(int fd):fdKey(fd) {

}

void worker::process() {
	session sess = lMap.get(fdKey);
	INFOF("iftp", "%s%d.\n", "process connection:", sess.ctrl_fd);

	rio_t rio;
	rio_readinitb(&rio, sess.ctrl_fd);
	while(1) {
		memset(sess.cmdLine, 0, sizeof(sess.cmdLine));
		memset(sess.cmd, 0, sizeof(sess.cmd));
		memset(sess.arg, 0, sizeof(sess.arg));
		int ret = rio_readlineb(&rio,sess.cmdLine, MAX_COMMAND_LINE);
		if(-1 == ret) {
			if(errno != EAGAIN) {
				/**
				 * 当出现错误时，则关闭控制连接且从map中移除session信息
				 * TODO： 可能后续还需关闭数据连接等
				 */
				ERROR("iftp", "fd %d: %s\n", sess.ctrl_fd, "rio_readlineb failed. close ctrl_fd and remove session from map.");
				if(-1 != sess.pasv_fd) {
					INFOF("iftp", "fd %d: %s%d.\n", sess.ctrl_fd, "close sess.pasv_fd ", sess.pasv_fd);
					close(sess.pasv_fd);
				}
				close(sess.ctrl_fd);
				lMap.remove(sess.ctrl_fd);
			} else {
				INFOF("iftp", "fd %d: %s\n", sess.ctrl_fd, "hasn't more data. ctrl_fd is added to epoll again.");
				/**
				 * 在将该连接放入epoll中重新进行监听之前，
				 * 需要将session的写回map中，因为下一次
				 * 处理该连接时，本次的worker就销毁了。
				 * 且将连接放回epoll之前，因为其不会被
				 * 别的进程获得并处理，就暂不需要写回。
				 */
				lMap.update(sess.ctrl_fd, sess);
				//该连接本次处理完成，加入epoll继续监听
				struct epoll_event ev;
				bzero(&ev, sizeof(struct epoll_event));
				ev.data.fd = sess.ctrl_fd;
				ev.events = EPOLLIN | EPOLLET;
				epoll_ctl(sess.epollfd, EPOLL_CTL_ADD, sess.ctrl_fd, &ev);
			}
			return;
		} else if(0 == ret) {
			/**
			 * 客户端关闭连接，则服务端也关闭连接，并移除session
			 */
			INFOF("iftp", "fd %d: %s\n", sess.ctrl_fd, "process connection finished. close ctrl_fd and remove session from map.");
			if(-1 != sess.pasv_fd) {
				INFOF("iftp", "fd %d: %s%d.\n", sess.ctrl_fd, "close sess.pasv_fd ", sess.pasv_fd);
				close(sess.pasv_fd);
			}
			close(sess.ctrl_fd);
			lMap.remove(sess.ctrl_fd);
			return;
		}
		//解析命令与参数(1)
		str_trim_crlf(sess.cmdLine);
		str_spilt(sess.cmdLine,sess.cmd,sess.arg,' ');
		str_upper(sess.cmd);

		INFOF("iftp", "%d%s%s.\n", sess.ctrl_fd, " cmdLine: ", sess.cmdLine);

		//处理命令(2)
		int i = 0;
		int length = sizeof(ctrl_cmds)/sizeof(struct ftpcmd_t);
		for(i = 0; i < length; i++){
			if(strcmp(ctrl_cmds[i].cmd, sess.cmd) == 0) {
				if(NULL != ctrl_cmds[i].cmd_handler) {
					ctrl_cmds[i].cmd_handler(sess);
				} else {
					WARN("iftp", "fd %d: %s\n", sess.ctrl_fd, "unimplemented command.");
					ftp_reply(sess, 502, "unimplemented command");
				}
				break;
			}
		}
		if(i == length)
			ftp_reply(sess, 500, "unknown cmd");
	}
}

void ftp_reply(session& sess, int status, const char *text) {
	char buf[1024] = {0};
	sprintf(buf, "%d %s.\r\n", status, text);
	rio_writen(sess.ctrl_fd, buf, strlen(buf));
}

static void do_user(session& sess) {
	struct passwd pw;
	struct passwd *result;
	char *buf;
	size_t bufsize;
	int s;

	bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (bufsize == -1)
		bufsize = 16384; 

	buf = new char[bufsize];
	if (buf == NULL) {
		ERROR("iftp", "%s\n", "malloc: malloc memory failed.");
		ftp_reply(sess, 530, "login incorrect");
		goto clear;
		return;
	}

	s = getpwnam_r(sess.arg, &pw, buf, bufsize, &result);
	if(NULL == result) {
		ERROR("iftp", "%s\n", "getpwuid: login incorrect.");
		ftp_reply(sess, 530, "login incorrect");
		goto clear;
		return;
	}

	//每次修改了session都需要将其写回map中
	sess.uid = pw.pw_uid;

	delete []buf;
	ftp_reply(sess, 331, "need password");
	return;
clear:
	if(buf != NULL)
		delete[] buf;
	// 同下
	close(sess.ctrl_fd);
}

static void do_pass(session& sess) {
	struct passwd pw;
	struct passwd *result;
	struct spwd sp;
	struct spwd *spResult;

	char *buf = NULL, *spbuf = NULL, *encrypted_pass = NULL;
	size_t bufsize, spbufsize = 16384;

	int s;
	bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (bufsize == -1)
		bufsize = 16384; 
	buf = new char[bufsize];
	if (NULL == buf) {
		ERROR("iftp", "%s\n", "new: malloc memory failed.");
		ftp_reply(sess, 530, "login incorrect");
		goto clear;
	}
	s = getpwuid_r(sess.uid, &pw, buf, bufsize, &result);
	if(NULL == result) {
		ERROR("iftp", "%s\n", "getpwuid: login incorrect.");
		ftp_reply(sess, 530, "login incorrect");
		goto clear;
	}

	spbuf = new char[spbufsize];
	if (NULL == spbuf) {
		ERROR("iftp", "%s\n", "new: malloc memory failed.");
		ftp_reply(sess, 530, "login incorrect");
		goto clear;
	}
	s = getspnam_r(pw.pw_name, &sp, spbuf, spbufsize, &spResult);
	if(NULL == spResult) {
		ERROR("iftp", "%s\n", "getspnam: login incorrect.");
		ftp_reply(sess, 530, "login incorrect");
		goto clear;
	}

	/*将明文密码加密*/
	encrypted_pass = crypt(sess.arg, sp.sp_pwdp);
	if(strcmp(encrypted_pass, sp.sp_pwdp) != 0) {
		ERROR("iftp", "%s\n", "crypt: login incorrect.");
		ftp_reply(sess, 530, "login incorrect");
		goto clear;
	}

	sess.gid = pw.pw_gid;
	strncpy(sess.dir, pw.pw_dir, PATH_MAX);	
	INFOF("iftp", "fd %d: %s%s.\n", sess.ctrl_fd, "home directory: ", sess.dir);

	delete[] buf;
	delete[] spbuf;
	ftp_reply(sess, 230, "login success");
	return;
clear: // 登录失败清除相应信息
	if(buf != NULL)
		delete[] buf;
	if(spbuf != NULL)
		delete[] spbuf;
	/**
	 * 在此关闭控制连接，则在主循环中
	 * 读取该连接则会出错，则会清除对
	 * 应表项。read一个关闭的描述符，
	 * 或关闭一个已关闭的描述符会返回
	 * 错误（EBADF），但在这这种错误
	 * 正是所需要的。
	 */
	close(sess.ctrl_fd);
}

static void do_syst(session& sess) {
	ftp_reply(sess,215,"Unix Type: L8");
}

static void do_feat(session& sess) {
	{
		char message[] = "211-Features:\r\n";
		rio_writen(sess.ctrl_fd, message, strlen(message));
	}
	{
		char message[] = " EPRT\r\n";
		rio_writen(sess.ctrl_fd, message, strlen(message));
	}
	{
		char message[] = " EPSV\r\n";
		rio_writen(sess.ctrl_fd, message, strlen(message));
	}
	{
		char message[] = " MDTM\r\n";
		rio_writen(sess.ctrl_fd, message, strlen(message));
	}
	{
		char message[] = " PASV\r\n";
		rio_writen(sess.ctrl_fd, message, strlen(message));
	}	
	{
		char message[] = " REST STREAM\r\n";
		rio_writen(sess.ctrl_fd, message, strlen(message));
	}
	{
		char message[] = " SIZE\r\n";
		rio_writen(sess.ctrl_fd, message, strlen(message));
	}	
	{
		char message[] = " TVFS\r\n";
		rio_writen(sess.ctrl_fd, message, strlen(message));
	}
	{
		char message[] = " UTF8\r\n";
		rio_writen(sess.ctrl_fd, message, strlen(message));
	}

	ftp_reply(sess, 211, "End");
}

static void do_pwd(session& sess) {
	ftp_reply(sess, 257, sess.dir);
}

static void do_type(session& sess) {
	if(strcmp(sess.arg, "A") == 0) {
		sess.is_ascii = true;
		INFOF("iftp", "fd %d: %s\n", sess.ctrl_fd, "switch to ASCII MODE.");
		ftp_reply(sess, 200, "switch to ASCII mode");
	} else if(strcmp(sess.arg, "I") == 0) {
		sess.is_ascii = false;
		INFOF("iftp", "fd %d: %s\n", sess.ctrl_fd, "switch to BINARY MODE.");
		ftp_reply(sess, 200, "switch to bianry mode");
	} else {
		WARN("iftp", "fd %d: %s\n", sess.ctrl_fd, "unrecognised type cmd.");
		ftp_reply(sess, 500, "unrecognised type cmd");
	}
}

static void do_port(session& sess) {
	if(NULL != sess.port_addr) {
		free(sess.port_addr);
		sess.port_addr = NULL;
		WARN("iftp", "fd %d: %s\n", sess.ctrl_fd, "do_pasv: double use port cmd; the older one be closed.");
	}

	unsigned int args[6];
	sscanf(sess.arg, "%u,%u,%u,%u,%u,%u", &args[2], &args[3], &args[4], &args[5], &args[0], &args[1]);
	sess.port_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	sess.port_addr->sin_family = AF_INET;
	unsigned char* p = (unsigned char*)&sess.port_addr->sin_port;
	p[0] = args[0];
	p[1] = args[1];

	p = (unsigned char *)&sess.port_addr->sin_addr;
	p[0] = args[2];
	p[1] = args[3];
	p[2] = args[4];
	p[3] = args[5];

	ftp_reply(sess, 200, "port cmd successful.using PASV");
}

static bool pasv_active(session& sess) {
	if(sess.pasv_fd != -1) {
/*		if(sess.port_addr != NULL){
			std::cerr << "pasv and port are both actived." << std::endl;
			PERR_EXIT("pasv_active");
		}*/
		return true;
	}
	return false;
}

static bool port_active(session& sess) {
	if(sess.port_addr != NULL) {
/*		if(sess.pasv_fd != -1){
			std::cerr << "pasv and port are both actived." << std::endl;
			PERR_EXIT("port_active");
		}*/
		return true;
	}
	return false;
}

static int get_port_fd(session& sess) {
	// port模式，服务端绑定20端口去，连接连接
	int fd = tcp_client(20, sess.ctrl_fd);
	if(-1 == fd){
		ERROR("iftp", "fd %d: %s\n", sess.ctrl_fd, "port mode: failed to bind port(20).");
		return 0;
	}

	if(connect_timeout_f(fd, sess.port_addr, connect_timeout) < 0) {
		close(fd);
		ERROR("iftp", "fd %d: %s\n", sess.ctrl_fd, "port mode: failed to connect to client.");
		return 0;
	}

	return fd;
}

int get_transfer_fd(session& sess) {
	if(!port_active(sess) && !pasv_active(sess)) {
		WARN("iftp", "fd %d: %s\n", sess.ctrl_fd, "must use port or pasv before transfering data.");
		return 0;
	}

	if(port_active(sess)) {
		int ret = get_port_fd(sess);
		// TODO:: 是否不deep copy port_addr?
		if(sess.port_addr) {
			free(sess.port_addr);
			sess.port_addr = NULL;
		}
		if(0 == ret) {
			return 0;
		}
		sess.data_fd = ret;
	}

	if(pasv_active(sess)){
		int ret = accept_timeout_f(sess.pasv_fd, accept_timeout);
		close(sess.pasv_fd);
		sess.pasv_fd = -1;
		if(-1 == ret){
			ERROR("iftp", "fd %d: %s\n", sess.ctrl_fd, "pasv mode: failed to accept connection.");
			return 0;
		}
		sess.data_fd = ret;
	}

	return 1;
}

static bool list_common(session& sess) {
	INFOF("iftp", "fd %d: %s%s.\n", sess.ctrl_fd, "list_commom: list dir ", sess.dir);
	DIR* dir = opendir(sess.dir);
	if(dir == NULL){
		ERROR("iftp", "fd %d: %s%s.\n", sess.ctrl_fd, "list_common: failed to opendir ", sess.dir);
		return false;
	}

	int baseFd = open(sess.dir, O_RDONLY);
	if(-1 == baseFd) {
		ERROR("iftp", "fd %d: %s%s.\n", sess.ctrl_fd, "list_common: failed to open ", sess.dir);
		return false;
	}

	struct dirent * dt;
	struct stat sbuf;
	char buf[1024];
	while((dt = readdir(dir))!=NULL) {
		memset(buf, 0, sizeof(buf));
		if(fstatat(baseFd, dt->d_name, &sbuf, AT_SYMLINK_NOFOLLOW) < 0) {
			WARN("iftp", "fd %d: %s%s.\n", sess.ctrl_fd, "list_common: lstat error on ", dt->d_name);
			continue;
		}
		if(dt->d_name[0] == '.')
			continue;
//		if(0 == strcmp(dt->d_name,".") || 0 == strcmp(dt->d_name,".."))
//			continue;
		char perm[] = "?---------";
		mode_t mode = sbuf.st_mode;
		switch(mode & S_IFMT){
		case S_IFREG:
			perm[0] = '-';
			break;
		case S_IFDIR:
			perm[0] = 'd';
			break;
		case S_IFLNK:
			perm[0] = 'l';
			break;
		case S_IFIFO:
			perm[0] = 'p';
			break;
		case S_IFSOCK:
			perm[0] = 's';
			break;
		case S_IFCHR:
			perm[0] = 'c';
			break;
		case S_IFBLK:
			perm[0] = 'b';
			break;
		}

		if(mode&S_IRUSR){
			perm[1] = 'r';
		}
		if(mode&S_IWUSR){
			perm[2] = 'w';
		}
		if(mode&S_IXUSR){
			perm[3] = 'x';
		}
		if(mode&S_IRGRP){
			perm[4] = 'r';
		}
		if(mode&S_IWGRP){
			perm[5] = 'w';
		}
		if(mode&S_IXGRP){
			perm[6] = 'x';
		}
		if(mode&S_IROTH){
			perm[7] = 'r';
		}
		if(mode&S_IWOTH){
			perm[8] = 'w';
		}
		if(mode&S_IXOTH){
			perm[9] = 'x';
		}
		if(mode&S_ISUID){
			perm[3] = (perm[3] == 'x') ? 's' : 'S';
		}
		if(mode&S_ISGID){
			perm[6] = (perm[6] == 'x') ? 's' : 'S';
		}
		if(mode&S_ISVTX){
			perm[9] = (perm[9] == 'x') ? 't' : 'T';
		}

		int off = sprintf(buf,"%s",perm);
		off += sprintf(buf+off," %3lu %-8d %-8d",sbuf.st_nlink,sbuf.st_uid,sbuf.st_gid);
		off += sprintf(buf+off," %8lu",sbuf.st_size);

		const char*  p_date_format = "%b %e %H:%M";
		struct timeval tv;
		gettimeofday(&tv,NULL);
		long local_time = tv.tv_sec;
		if(sbuf.st_mtime > local_time || (local_time - sbuf.st_mtime) > 3600*24*182){
			p_date_format = "%b %e %Y";
		}

		char datebuf[64] = {0};
		struct tm* p_tm = localtime(&sbuf.st_mtime);
		strftime(datebuf,sizeof(datebuf),p_date_format,p_tm);

		off += sprintf(buf+off," %s",datebuf);

		if((mode & S_IFMT) == S_IFLNK){
			char linkbuf[1024] = {0};
			readlink(dt->d_name, linkbuf,sizeof(linkbuf));
			sprintf(buf+off," %s -> %s\r\n",dt->d_name,linkbuf);
		}else{
			sprintf(buf+off," %s\r\n",dt->d_name);
		}
		rio_writen(sess.data_fd,buf,strlen(buf));
	}
	return true;
}

static void do_list(session& sess) {
	if(get_transfer_fd(sess) == 0) {
		ftp_reply(sess, 425, "use pasv or port first");
		return;
	}

	ftp_reply(sess, 150, "here comes the directory listing");
	if(!list_common(sess)) {
		ERROR("iftp", "fd %d: %s\n", sess.ctrl_fd, "list: failed to list.");
		ftp_reply(sess, 451, "Requested action aborted");
		goto clear;
	}

	ftp_reply(sess, 226, "directory send ok");
clear:
	close(sess.data_fd);
	sess.data_fd = -1;
}

static void do_pasv(session& sess){
	if(-1 != sess.pasv_fd) {
		close(sess.pasv_fd);
		sess.pasv_fd = -1;
		WARN("iftp", "fd %d: %s\n", sess.ctrl_fd, "do_pasv: double use pasv cmd; the older one be closed.");
	}

	char ip[16] = {0};

	if(!getlocalip(ip, sess.ctrl_fd)) {
		ERROR("iftp", "fd %d: %s\n", sess.ctrl_fd, "pasv mode: failed to get local ip.");
		ftp_reply(sess, 451, "Requested action aborted");
		return;
	}

	sess.pasv_fd = tcp_server(ip, 0);
	if(-1 == sess.pasv_fd) {
		ERROR("iftp", "fd %d: %s\n", sess.ctrl_fd, "pasv mode: failed to create listenfd.");
		ftp_reply(sess, 451, "Requested action aborted");
		return;
	}

	// 将本地监听地址发送给client
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	if(getsockname(sess.pasv_fd, (struct sockaddr*)&addr, &addrlen) < 0) {
		ERROR("iftp", "fd %d: %s\n", sess.ctrl_fd, "pasv mode: failed to get local port.");
		close(sess.pasv_fd);
		sess.pasv_fd = -1;
		ftp_reply(sess, 451, "Requested action aborted");
		return;
	}
	unsigned port = ntohs(addr.sin_port);
	unsigned int args[4];
	sscanf(ip, "%u.%u.%u.%u.", &args[0], &args[1], &args[2], &args[3]);
	char text[1024] = {0};
	sprintf(text, "Enter Passive Mode (%u,%u,%u,%u,%u,%u)", args[0], args[1], args[2], args[3], port>>8, port&0xFF);

	ftp_reply(sess, 227, text);
}


static void do_cwd(session& sess) {
	struct stat sbuf;
	if(-1 == stat(sess.arg, &sbuf)) {
		INFOF("iftp", "fd %d: %s\n", "do_cwd: the arg may be a  relative path. try it.");
		int baseFd = open(sess.dir, O_RDONLY);
		if(-1 == baseFd) {
			ERROR("iftp", "fd %d: %s%s.\n", sess.ctrl_fd, "do_cwd: failed to open ", sess.dir);
			ftp_reply(sess, 550, "failed to change directory");
			return;
		}
		if(fstatat(baseFd, sess.arg, &sbuf, 0) < 0) {
			ERROR("iftp", "fd %d: %s%s/%s.\n", sess.ctrl_fd, "do_cwd: failed to fstatat ", sess.dir, sess.arg);
			ftp_reply(sess, 550, "failed to change directory");
			return;
		}
		if(((sbuf.st_uid == sess.uid) && (S_IXUSR & sbuf.st_mode)) || ((sbuf.st_gid == sess.uid) && (S_IXGRP & sbuf.st_mode)) || (S_IXOTH & sbuf.st_mode)) {
			sess.dir[strlen(sess.dir)] = '/';
			strncpy(sess.dir+strlen(sess.dir), sess.arg, PATH_MAX - strlen(sess.dir));
			INFOF("iftp", "fd %d: %s%s.\n", sess.ctrl_fd, "do_cwd: user can access ", sess.dir);
		} else {
			ERROR("iftp", "fd %d: %s%s.\n", sess.ctrl_fd, "do_cwd: user can't access ", sess.dir);
			ftp_reply(sess, 550, "failed to change directory");
			return;
		}
	} else {
		if(((sbuf.st_uid == sess.uid) && (S_IXUSR & sbuf.st_mode)) || ((sbuf.st_gid == sess.uid) && (S_IXGRP & sbuf.st_mode)) || (S_IXOTH & sbuf.st_mode)) {
			strncpy(sess.dir, sess.arg, PATH_MAX);
			INFOF("iftp", "fd %d: %s%s.\n", sess.ctrl_fd, "do_cwd: user can access ", sess.arg);
		} else {
			ERROR("iftp", "fd %d: %s%s.\n", sess.ctrl_fd, "do_cwd: user can't access ", sess.arg);
			ftp_reply(sess, 550, "failed to change directory");
			return;
		}
	}
	ftp_reply(sess, 250, "directory successfully changed");
}