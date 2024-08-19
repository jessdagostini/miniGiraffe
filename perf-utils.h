#ifndef TIMEUTILS_H
#define TIMEUTILS_H

#include <iostream>
#include <omp.h>
#include <pthread.h>

#include "uthash.h"

using namespace std;

struct entryPoint {
    long long int id;
    int id_func;
    double value;
    int thread;
    UT_hash_handle hh;
};

extern entryPoint *events;

extern long long int entryPointCount;

extern pthread_rwlock_t lock_time_utils;

extern char names[10][40];

void perf_utils_dump();

void perf_utils_add(double value, int id_func, int thread);
#endif