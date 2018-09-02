#ifndef _worker_
#define _worker_

void ftp_reply(session& sess, int status, const char *text);

class worker {
    session sess;
public:
    worker(const session& sess);
    void process();
};

#endif