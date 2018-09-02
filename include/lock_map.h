#ifndef _lock_map_
#define _lock_map_

class lock_map {
    pthread_rwlock_t m_rdlock;
    std::unordered_map<int, session> map;
public:
    lock_map();
    ~lock_map();
    session get(int);
    void put(int, const session&);
    void remove(int);
    void update(int, const session&);
private:
    lock_map(const lock_map&);
    lock_map& operator=(const lock_map&);
};

#endif