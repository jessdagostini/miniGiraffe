#include "time-utils.h"

entryPoint * timers = NULL;

long long int entryPointCount = 0;

pthread_rwlock_t lock_time_utils;


void time_utils_dump() {
    // fprintf(stderr, "Entrou dump\n");
    entryPoint *s = NULL;
    entryPoint *tmp = NULL;

    // fprintf(stderr, "\nTime Measurements\n");
    HASH_ITER(hh, timers, s, tmp) {
        fprintf(stderr, "%s, %0.10f, %0.10f, %d, %d\n", regions[s->id_func], s->start, s->end, s->thread, s->rank);
    }
}

void time_utils_add(double start, double end, int id_func, int rank, int thread) {
    if (pthread_rwlock_wrlock(&lock_time_utils) != 0) {
        fprintf(stderr, "Can't get mutex\n");
        exit(-1);
    }

    entryPoint *s = (entryPoint *) malloc(sizeof *s);

    s->start = start;
    s->end = end;
    s->thread = thread;
    s->rank = rank;
    s->id_func = id_func;
    s->id = entryPointCount;

    HASH_ADD_INT(timers, id, s);  /* id is the key field */

    entryPointCount++;

    pthread_rwlock_unlock(&lock_time_utils);
    // pthread_mutex_unlock(&incoming_queue_mutex);
}

void time_utils_add(double duration, int id_func) {
    if (pthread_rwlock_wrlock(&lock_time_utils) != 0) {
        fprintf(stderr, "Can't get mutex\n");
        exit(-1);
    }

    entryPoint *s = (entryPoint *) malloc(sizeof *s);

    s->start = 0.0;
    s->end = duration;
    s->thread = 0;
    s->id_func = id_func;
    s->id = entryPointCount;

    HASH_ADD_INT(timers, id, s);  /* id is the key field */

    entryPointCount++;

    pthread_rwlock_unlock(&lock_time_utils);
    // pthread_mutex_unlock(&incoming_queue_mutex);
}