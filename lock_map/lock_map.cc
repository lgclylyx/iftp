#include "common.h"
#include "log.h"
#include "session.h"
#include "lock_map.h"

lock_map::lock_map() {
    pthread_rwlock_init(&m_rdlock,NULL);
}

lock_map::~lock_map() {
    pthread_rwlock_destroy(&m_rdlock);
}

session lock_map::get(int key) {
    session ret;
    pthread_rwlock_rdlock(&m_rdlock);
    if(map.find(key) != map.end())
        ret = map[key];
    pthread_rwlock_unlock(&m_rdlock);
    return ret;
}

void lock_map::put(int key, const session& sess) {
    pthread_rwlock_wrlock(&m_rdlock);
    map[key] = sess;
    pthread_rwlock_unlock(&m_rdlock);
}

void lock_map::remove(int key) {
    pthread_rwlock_wrlock(&m_rdlock);
    if(map.find(key) != map.end())
        map.erase(key);
    pthread_rwlock_unlock(&m_rdlock);
}

void lock_map::update(int key, const session& sess) {
    pthread_rwlock_wrlock(&m_rdlock);
    if(map.find(key) != map.end())
        map[key] = sess;
    pthread_rwlock_unlock(&m_rdlock);
}