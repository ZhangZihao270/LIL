#include "pthread_barrier_t.h"

#include <errno.h>
#ifndef __linux__
int
pthread_barrier_init(pthread_barrier_t * barrier, const pthread_barrier_t * attr __unused,
             unsigned int count)
{
    if (count == 0) {
        errno = EINVAL;
        return -1;
    }

    if (pthread_mutex_init(&barrier->mutex, 0) < 0) {
        return -1;
    }
    if (pthread_cond_init(&barrier->cond, 0) < 0) {
        int errno_save = errno;
        pthread_mutex_destroy(&barrier->mutex);
        errno = errno_save;
        return -1;
    }

    barrier->limit = count;
    barrier->count = 0;
    barrier->phase = 0;

    return 0;
}

int
pthread_barrier_destroy(pthread_barrier_t *barrier)
{
    pthread_mutex_destroy(&barrier->mutex);
    pthread_cond_destroy(&barrier->cond);
    return 0;
}

int
pthread_barrier_wait(pthread_barrier_t *barrier)
{
    pthread_mutex_lock(&barrier->mutex);
    barrier->count++;
    if (barrier->count >= barrier->limit) {
        barrier->phase++;
        barrier->count = 0;
        pthread_cond_broadcast(&barrier->cond);
        pthread_mutex_unlock(&barrier->mutex);
        return PTHREAD_BARRIER_SERIAL_THREAD;
    } else {
        unsigned phase = barrier->phase;
        do
            pthread_cond_wait(&barrier->cond, &barrier->mutex);
        while (phase == barrier->phase);
        pthread_mutex_unlock(&barrier->mutex);
        return 0;
    }
}

#endif
