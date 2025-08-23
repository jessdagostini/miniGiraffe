#ifndef HANDLEUTILS_H
#define HANDLEUTILS_H

#include <iostream>
#include <omp.h>

#include "uthash.h"

using namespace std;

struct handleEntryPoint {
    long long int id;
    long long int node;
    int thread;
    UT_hash_handle hh;
};

extern handleEntryPoint *nodes;

extern long long int handleEntryPointCount;

extern pthread_rwlock_t lock_handle_utils;

void handle_utils_dump();

void handle_utils_add(long long int value, int thread);
#endif