#ifndef __QEMU_THREAD_POSIX_H
#define __QEMU_THREAD_POSIX_H 1
#include "pthread.h"
#include "config-host.h"

struct QemuMutex {
    pthread_mutex_t lock;
};

struct QemuCond {
    pthread_cond_t cond;
};

struct QemuThread {
    pthread_t thread;
};

#ifndef CONFIG_DARWIN

struct QemuSpinlock {
    pthread_spinlock_t lock;
};

#endif

#endif
