#include "perf-utils.h"

entryPoint * events = NULL;

long long int entryPointCount = 0;

pthread_rwlock_t lock_time_utils;

// For gbwt_extender
// char names[10][40] = {"cycles", "kcycles", "instructions", "L1-acess", "L1-misses", "LL-access", "LL-misses", "LLC-misses", "branch-misses", "task-clock"};
char names[10][40] = {"LL-access", "LL-misses"};

void perf_utils_dump() {
    // fprintf(stderr, "Entrou dump\n");
    entryPoint *s = NULL;
    entryPoint *tmp = NULL;

    HASH_ITER(hh, events, s, tmp) {
        fprintf(stderr, "%s, %0.10f, %d\n", names[s->id_func], s->value, s->thread);
    }
}

void perf_utils_add(double value, int id_func, int thread) {
    if (pthread_rwlock_wrlock(&lock_time_utils) != 0) {
        fprintf(stderr, "Can't get mutex\n");
        exit(-1);
    }

    entryPoint *s = (entryPoint *) malloc(sizeof *s);

    s->value = value;
    s->thread = thread;
    s->id_func = id_func;
    s->id = entryPointCount;

    HASH_ADD_INT(events, id, s);  /* id is the key field */

    entryPointCount++;

    pthread_rwlock_unlock(&lock_time_utils);
    // pthread_mutex_unlock(&incoming_queue_mutex);
}