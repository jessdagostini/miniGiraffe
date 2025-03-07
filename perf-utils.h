#ifndef TIMEUTILS_H
#define TIMEUTILS_H

#include <iostream>
#include <omp.h>
#include <pthread.h>

#include "uthash.h"

using namespace std;

struct perfEntryPoint {
    long long int id;
    int id_func;
    double value;
    int thread;
    UT_hash_handle hh;
};

extern perfEntryPoint *events;

extern long long int perfEntryPointCount;

extern pthread_rwlock_t lock_perf_utils;

char counters[20][40] = {"cycles", "instructions", "L1-access", "L1-misses", "LLC-access", "LLC-misses", "branch-issued", "branch-misses", "DTLB-access", "DTLB-misses", "ITLB-access", "ITLB-misses"};

enum PerfUtilsCounters {
    CYCLES,
    INSTRUCTIONS,
    L1ACCESS,
    L1MISSES,
    LLCACCESS,
    LLCMISSES,
    BRANCHISSUED,
    BRANCHMISSES,
    DTLBACCESS,
    DTLBMISSES,
    ITLBACCESS,
    ITLBMISSES
};

void perf_utils_dump();

void perf_utils_add(double value, int id_func, int thread);
#endif