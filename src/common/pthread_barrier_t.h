#ifndef PTHREAD_BARRIER_T_H
#define PTHREAD_BARRIER_T_H


#include <pthread.h>

#ifndef __linux__

#if !defined(PTHREAD_BARRIER_SERIAL_THREAD)
# define PTHREAD_BARRIER_SERIAL_THREAD  (1)
#endif

#if !defined(PTHREAD_PROCESS_PRIVATE)
# define PTHREAD_PROCESS_PRIVATE    (42)
#endif
#if !defined(PTHREAD_PROCESS_SHARED)
# define PTHREAD_PROCESS_SHARED     (43)
#endif

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    unsigned int limit;
    unsigned int count;
    unsigned int phase;
} pthread_barrier_t;

int pthread_barrier_init(pthread_barrier_t * barrier, const pthread_barrier_t * attr, unsigned int count);
int pthread_barrier_destroy(pthread_barrier_t *barrier);

int pthread_barrier_wait(pthread_barrier_t *barrier);

#endif

#endif // PTHREAD_BARRIER_T_H
