#include "metric-utils.h"

entryPoint* timers = NULL;
perfEntryPoint* events = NULL;

long long int entryTimeCount = 0;
long long int perfEntryPointCount = 0;

pthread_rwlock_t lock_time_utils;
pthread_rwlock_t lock_perf_utils;


void time_utils_dump() {
    entryPoint *s = NULL;
    entryPoint *tmp = NULL;

    HASH_ITER(hh, timers, s, tmp) {
        fprintf(stderr, "%s, %0.10f, %0.10f, %d\n", regions[s->id_func], s->start, s->end, s->thread);
    }
}

void perf_utils_dump() {
    perfEntryPoint *s = NULL;
    perfEntryPoint *tmp = NULL;
    HASH_ITER(hh, events, s, tmp) {
        fprintf(stderr, "%s, %0.10f, %d\n", counters[s->id_func], s->value, s->thread);
    }
}

void time_utils_add(double start, double end, int id_func, int thread) {
    if (pthread_rwlock_wrlock(&lock_time_utils) != 0) {
        fprintf(stderr, "Can't get mutex\n");
        exit(-1);
    }

    entryPoint *s = (entryPoint *) malloc(sizeof *s);

    s->start = start;
    s->end = end;
    s->thread = thread;
    s->id_func = id_func;
    s->id = entryTimeCount;

    HASH_ADD_INT(timers, id, s);  /* id is the key field */

    entryTimeCount++;

    pthread_rwlock_unlock(&lock_time_utils);
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
    s->id = entryTimeCount;

    HASH_ADD_INT(timers, id, s);  /* id is the key field */

    entryTimeCount++;

    pthread_rwlock_unlock(&lock_time_utils);
}

void perf_utils_add(double value, int id_func, int thread) {
    if (pthread_rwlock_wrlock(&lock_perf_utils) != 0) {
        fprintf(stderr, "Can't get mutex\n");
        exit(-1);
    }

    perfEntryPoint *s = (perfEntryPoint *) malloc(sizeof *s);

    s->value = value;
    s->thread = thread;
    s->id_func = id_func;
    s->id = perfEntryPointCount;

    HASH_ADD_INT(events, id, s);  /* id is the key field */

    perfEntryPointCount++;

    pthread_rwlock_unlock(&lock_perf_utils);
}