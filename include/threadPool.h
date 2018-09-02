#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

class sem {
public:
    sem() {
        sem_init(&m_sem, 0, 0);
    }
    ~sem() {
        sem_destroy(&m_sem);
    }
    bool wait() {
        return sem_wait(&m_sem) == 0;
    }
    bool post() {
        return sem_post(&m_sem) == 0;
    }
private:
    sem_t m_sem;
};

class locker {
public:
    locker() {
        pthread_mutex_init( &m_mutex, NULL );
    }

    ~locker() {
        pthread_mutex_destroy( &m_mutex );
    }
    bool lock() {
        return pthread_mutex_lock( &m_mutex ) == 0;
    }
    bool unlock() {
        return pthread_mutex_unlock( &m_mutex ) == 0;
    }
private:
    pthread_mutex_t m_mutex;
};

template<typename T>
class threadpool {
public:
    threadpool(int thread_number = 20, int max_requests = 10000);
    ~threadpool();
    bool append(T* request);

private:
    static void* worker(void* arg);
    void run();

    int m_thread_number;
    int m_max_requests;
    pthread_t* m_threads;
    std::list<T*> m_workqueue;
    locker m_queuelocker;
    sem m_queuestat;
    bool m_stop;
};

template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) : 
        m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(NULL) {
    if(( thread_number <= 0 ) || ( max_requests <= 0 )) {
        ERROR("iftp", "%s: thread_num: %d, max_requests: %d.\n", "error args", thread_number, max_requests);
        exit(EXIT_FAILURE);
    }

    m_threads = new pthread_t[ m_thread_number ];
    if(! m_threads) {
        ERROR("iftp", "%s\n", "new failed.");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < thread_number; ++i) {
        INFOF("iftp", "create the %dth thread\n", i);
        if(pthread_create(m_threads + i, NULL, worker, this) != 0) {
            delete [] m_threads;
            ERROR("iftp", "%s\n", "pthread_create failed.");
            exit(EXIT_FAILURE);
        }
        if(pthread_detach(m_threads[i])) {
            delete [] m_threads;
            ERROR("iftp", "%s\n", "pthread_detach failed.");
            exit(EXIT_FAILURE);
        }
    }
}

template<typename T>
threadpool<T>::~threadpool() {
    delete [] m_threads;
    m_stop = true;
}

/**
 * 往队列中放入的任务要是使用new创建的T对象
 * 因为，后续会delete T。
 */
template<typename T>
bool threadpool<T>::append(T* request) {
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<typename T>
void* threadpool<T>::worker(void* arg) {
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run() {
    while (! m_stop) {
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (! request) {
            continue;
        }
        request->process();
        /**
         * 当往任务队列中放入任务时，会new一个T对象，
         * 此时该任务执行完毕，则该T对象就需要释放，
         * 不然会造成内存泄漏。
         */
        delete request;
    }
}

#endif
