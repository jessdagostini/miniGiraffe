// #ifndef TIMEUTILS_H
// #define TIMEUTILS_H

#include <iostream>
#include <omp.h>
#include <pthread.h>

#include "uthash.h"

using namespace std;

struct entryPoint {
    long long int id;
    int id_func;
    double start;
    double end;
    int rank;
    int thread;
    UT_hash_handle hh;
};

extern entryPoint *timers;

extern long long int entryPointCount;

extern pthread_rwlock_t lock_time_utils;

char regions[5][40] = {"reading-seeds", "reading-gbz", "writing-output", "seeds-loop", "sorting-seeds"};

enum TimeUtilsRegions {
    READING_SEEDS,      // Automatically assigned 0
    READING_GBZ,        // Automatically assigned 1
    WRITING_OUTPUT,     // Automatically assigned 2
    SEEDS_LOOP,         // Automatically assigned 3
    SORTING_SEEDS       // Automatically assigned 4
};

void time_utils_dump();

void time_utils_add(double start, double end, int id_func, int rank, int thread);
void time_utils_add(double duration, int id_func);
// #endif