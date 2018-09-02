#ifndef _worker_
#define _worker_

void ftp_reply(session& sess, int status, const char *text);

class worker {
    int fdKey;
public:
    worker(int fd);
    void process();
};

#endif