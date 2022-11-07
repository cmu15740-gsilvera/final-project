#pragma once

#include "sync_modes.h" // SyncMode enum
#include <iostream>

bool verbose = true; // disable with 4th optional param

pthread_mutex_t stdout_lock; // stdout_lock is just for pretty printing to stdout (cout)

#define cout_lock(x)                                                                                                   \
    pthread_mutex_lock(&stdout_lock);                                                                                  \
    if (verbose)                                                                                                       \
        std::cout << x << std::endl;                                                                                   \
    pthread_mutex_unlock(&stdout_lock);

typedef uint64_t cycles_t;
static inline cycles_t get_cycles()
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts))
        return -1ULL;
    // assuming nanoseconds ~ cycles (approximately true)
    return ((uint64_t)ts.tv_sec * 1000000000ULL) + ts.tv_nsec;
}
