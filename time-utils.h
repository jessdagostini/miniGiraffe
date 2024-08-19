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
    int thread;
    UT_hash_handle hh;
};

extern entryPoint *timers;

extern long long int entryPointCount;

extern pthread_rwlock_t lock_time_utils;

extern char regions[10][40];

void time_utils_dump();

void time_utils_add(double start, double end, int id_func, int thread);
void time_utils_add(double duration, int id_func);
// #endif

// class TimeUtils {
//     public:
//         struct entryPoint {
//             double start;
//             double end;
//             string func;
//             int thread;
//         };

//         vector<entryPoint> snapshots;
//         entryPoint e1;
//         entryPoint e2;
//         entryPoint e3;
        
//         TimeUtils() = default;

//         void setStart1(string func) {
//            e1.start = omp_get_wtime();
//            e1.func = func;
//            e1.thread = omp_get_thread_num();
//         }

//         void duration1(string func) {
//             e1.end = omp_get_wtime();
//             snapshots.push_back(e1);
//             e1 = {};
//         }

//         void setStart2(string func) {
//            e2.start = omp_get_wtime();
//            e2.func = func;
//            e2.thread = omp_get_thread_num();
//         }

//         void duration2(string func) {
//             e2.end = omp_get_wtime();
//             snapshots.push_back(e2);
//             e2 = {};
//         }

//         void setStart3(string func) {
//            e3.start = omp_get_wtime();
//            e3.func = func;
//            e3.thread = omp_get_thread_num();
//         }

//         void duration3(string func) {
//             e3.end = omp_get_wtime();
//             snapshots.push_back(e3);
//             e3 = {};
//         }

//         void dump() {
//             for (auto eP = snapshots.begin(); eP != snapshots.end(); ++eP) {
//                 fprintf(stderr, "%s,%.10f,%.10f,%d\n", eP->func.c_str(), eP->start, eP->end, eP->thread);
//             }
//         }
// };