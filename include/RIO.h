#ifndef RIO_USE_H
#define RIO_USE_H



#if defined (__cplusplus)
extern "C" {
#endif

#define RIO_BUFSIZE 8192

typedef struct{
	int rio_fd;
	int rio_cnt;
	char* rio_bufptr;
	char rio_buf[RIO_BUFSIZE];
}rio_t;

int rio_readn(int fd, void* usrbuf, size_t n);
int rio_writen(int fd, void* usrbuf, size_t n);
void rio_readinitb(rio_t *rp, int fd) ;
int rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);
int rio_readnb(rio_t *rp, void *usrbuf, size_t n);

#if defined (__cplusplus)
}
#endif

#endif
