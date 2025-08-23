#include "handle-utils.h"

handleEntryPoint * nodes = NULL;

long long int handleEntryPointCount = 0;

pthread_rwlock_t lock_handle_utils;

void handle_utils_dump() {
    // fprintf(stderr, "Entrou dump\n");
    handleEntryPoint *s = NULL;
    handleEntryPoint *tmp = NULL;
    // fprintf(stderr, "\nHardware Measurements\n");
    HASH_ITER(hh, nodes, s, tmp) {
        fprintf(stderr, "%lld, %d\n", s->node, s->thread);
    }
}

void handle_utils_add(long long int node, int thread) {
    if (pthread_rwlock_wrlock(&lock_handle_utils) != 0) {
        fprintf(stderr, "Can't get mutex\n");
        exit(-1);
    }

    handleEntryPoint *s = (handleEntryPoint *) malloc(sizeof *s);

    s->node = node;
    s->thread = thread;
    s->id = handleEntryPointCount;

    HASH_ADD_INT(nodes, id, s);  /* id is the key field */

    handleEntryPointCount++;

    pthread_rwlock_unlock(&lock_handle_utils);
    // pthread_mutex_unlock(&incoming_queue_mutex);
}