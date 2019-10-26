#include "common.h"
#include "log.h"
#include "session.h"
#include "lock_map.h"
#include "worker.h"
#include "threadPool.h"

lock_map lMap;
threadpool<worker> pool;
