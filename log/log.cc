#include "common.h"

class logger {
    pthread_rwlock_t m_rdlock;
    std::unordered_map<std::string, int> map;
public:
    logger();
    ~logger();
    int get(const std::string&);
private:
    logger(const logger&);
    logger& operator=(const logger&);
};

logger::logger() {
    pthread_rwlock_init(&m_rdlock,NULL);
}

logger::~logger() {
    pthread_rwlock_destroy(&m_rdlock);
}

/**
 *获取文件名为key的log文件描述符，如果该log文件
 *不存在则新建该文件 
 */
int logger::get(const std::string& key) {
    int retfd = -1;
    /*
    *先加上读锁，去获取对应log文件的描述符
    */
    pthread_rwlock_rdlock(&m_rdlock);
    if(map.find(key) != map.end())
        retfd = map[key];
    pthread_rwlock_unlock(&m_rdlock);
    /*
    *如果该文件不存在，则打开或创建该文件，
    *此时，应该加上写锁
    */
    if(retfd == -1) {
        pthread_rwlock_wrlock(&m_rdlock);
        /*
        *因为读写锁之间不是原子的，则需要再
        *判断该文件是否已被打开
        */
        if(map.find(key) != map.end()) {
            retfd = map[key];
            pthread_rwlock_unlock(&m_rdlock);
            return retfd;
        }
        retfd = open(key.c_str(),  O_WRONLY | O_APPEND | O_CREAT | O_TRUNC, 0666);
        if(-1 != retfd) {
            map[key] = retfd;
        } else {
            retfd = open("/dev/null", O_WRONLY);
        }
        pthread_rwlock_unlock(&m_rdlock);
    }
    return retfd;
}

static std::string getKey(const std::string& name) {
    return "log." + name + ".dat";
}

void output(const char *file, int line, const std::string& name, const std::string& level, const char *fmt, ...) {
    static logger log;
    int fd = log.get(getKey(name));
    

    va_list		ap;
    int		off = 0;
    char	buff[1024];

    off = snprintf(buff, sizeof(buff), "%s: ", level.c_str());
    time_t now = time(NULL);
    off += strftime(buff + off, sizeof(buff) - off, "%Y-%m-%d %H:%M:%S ", localtime(&now));
    off += snprintf(buff + off, sizeof(buff) - off, "(%s:%d:%lu) ",file, line, pthread_self());
    va_start(ap, fmt);
    vsnprintf(buff + off, sizeof(buff) - off, fmt, ap);
    va_end(ap);

    write(fd, buff, strlen(buff));
}