#include "common.h"
#include "conf.h"
#include "log.h"
#include "inet_util.h"
#include "session.h"
#include "lock_map.h"
#include "worker.h"
#include "threadPool.h"
#include "global.h"

int main(int argc, char* argv[]){
	parseconf_load_file(IFTP_CONFIG_PATH);

	if(0 != getuid()){
		ERROR("iftp", "%s\n", "iftpd: must be started by root user...");
		exit(EXIT_FAILURE);
	}

	std::string real_ipaddress;
	if(NULL == listen_address)
		real_ipaddress = "";
	else
		real_ipaddress = listen_address;

    int epollfd = epoll_create(max_clients + 1);
    if(epollfd < 0) {
        ERROR("iftp", "%s\n", "epoll_create: failed to create epoll.");
        PERR_EXIT("epoll_create");
    }

	int listenfd = tcp_server(real_ipaddress,listen_port);
    if(-1 == listenfd) {
        ERROR("iftp", "%s\n", "tcp_server: failed to create tcp server.");
        exit(EXIT_FAILURE);
    }
    
    struct epoll_event ev;
    // 将listenfd加入epoll监听
	bzero(&ev, sizeof(struct epoll_event));
	ev.data.fd = listenfd;
	ev.events = EPOLLIN;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev);
    INFOF("iftp", "%s: %s:%d.\n", "listen on", real_ipaddress.c_str(), listen_port);

    struct sockaddr_in clientaddr;
    socklen_t addrlen = sizeof(struct sockaddr);

	while(1){
        struct epoll_event events[max_clients + 1];
        memset(events, 0, sizeof(events));

        int err = epoll_wait(epollfd, events, max_clients + 1, -1);
        if(err < 0) {
            INFOF("iftp", "%s\n", "epoll_wait failed.");
            continue;
        }
        
        for(int i = 0; i < err; i++) {
            int fd = events[i].data.fd;
            if(fd == listenfd) {
                INFOF("iftp", "%s\n", "listenfd: a new connection coming.");
                bzero(&clientaddr, sizeof(struct sockaddr));
		        int connfd = accept(listenfd, (struct sockaddr*)&clientaddr, &addrlen);
                if(connfd < 0) {
                    INFOF("iftp", "%s\n", "listenfd: accept failed.");
                    continue;
                }
                {
                    //设置为非阻塞模式，从而能够达到I/O复用额目的                 
                    setNoblock(connfd, 1);
                }
                {
                    bzero(&ev, sizeof(struct epoll_event));
                    ev.data.fd = connfd;
                    ev.events = EPOLLIN | EPOLLET;
                    epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev);
                }
                {
                    //TODO: 初始化sess
                    session sess;
                    sess.ctrl_fd = connfd;
                    sess.epollfd = epollfd;
                    lMap.put(connfd, sess);
                    /**
                     * 发送welcome信息，如果不发送，无法进行接下
                     * 来的步骤，会使客户端阻塞
                     */
                    ftp_reply(sess, 220, "(iftp v1.0))");
                }
            } else {
                if(events[i].events & EPOLLIN) {
                    {
                        struct epoll_event ev;
						ev.data.fd = fd;
						epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
                    }
                    INFOF("iftp", "%d: %s\n", fd, "send a new cmdLine.");
                    worker* w = new worker(fd);
                    pool.append(w);
                }
            }
        }
	}
	return 0;
}