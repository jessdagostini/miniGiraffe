#include "perf-utils.h"

perfEntryPoint * events = NULL;

long long int perfEntryPointCount = 0;

pthread_rwlock_t lock_perf_utils;

void perf_utils_dump() {
    // fprintf(stderr, "Entrou dump\n");
    perfEntryPoint *s = NULL;
    perfEntryPoint *tmp = NULL;
    // fprintf(stderr, "\nHardware Measurements\n");
    HASH_ITER(hh, events, s, tmp) {
        fprintf(stderr, "%s, %0.10f, %d\n", counters[s->id_func], s->value, s->thread);
    }
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
    // pthread_mutex_unlock(&incoming_queue_mutex);
}