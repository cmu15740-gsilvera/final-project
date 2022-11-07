#pragma once

#include "sync_modes.h"
#include "utils.h"
#include <iostream>

typedef size_t counter_t;
counter_t *gbl_counter = new counter_t(0); // this is the global!

std::atomic<counter_t> gbl_counter_atomic{0};
pthread_rwlock_t rwlock;   // reader-writer lock (supports concurrent readers)
pthread_mutex_t mutexlock; // single user (reader or writer) mutex

inline void update_counter()
{
    switch (sync_method)
    {
    case (SyncMethod::RCU): {
        // similar to
        // https://www.kernel.org/doc/html/latest/RCU/whatisRCU.html#what-are-some-example-uses-of-core-rcu-api
        counter_t *new_counter;
        counter_t *old_counter;
        new_counter = new counter_t{0};
        pthread_mutex_lock(&mutexlock);
        old_counter = gbl_counter;                                 // copy ptr of global
        *new_counter = (*old_counter + 1);                         // bump global's value to local new
        old_counter = rcu_xchg_pointer(&gbl_counter, new_counter); // swap with global
        pthread_mutex_unlock(&mutexlock);
        synchronize_rcu(); // synchronize_rcu();
        delete old_counter;
        break;
    }
    case (SyncMethod::RWLOCK): {
        pthread_rwlock_wrlock(&rwlock); // lock for writing
        (*gbl_counter)++;               // bump
        pthread_rwlock_unlock(&rwlock);
        break;
    }
    case (SyncMethod::LOCK): {
        pthread_mutex_lock(&mutexlock); // lock for writing
        (*gbl_counter)++;               // bump
        pthread_mutex_unlock(&mutexlock);
        break;
    }
    case (SyncMethod::ATOMIC): {
        gbl_counter_atomic++; // bump
        break;
    }
    case (SyncMethod::RACE): {
        (*gbl_counter)++; // bump
        break;
    }
    default:
        throw std::runtime_error("Not implemented!");
    }
}

inline void read_counter(counter_t &var)
{
    counter_t old_val = var;
    switch (sync_method)
    {
    case (SyncMethod::RCU): {
        _rcu_read_lock();
        counter_t *local_ptr = nullptr;
        local_ptr = _rcu_dereference(gbl_counter);
        if (local_ptr)
            var = (*local_ptr);
        _rcu_read_unlock();
        break;
    }
    case (SyncMethod::RWLOCK): {
        pthread_rwlock_rdlock(&rwlock); // lock for reading
        var = (*gbl_counter);
        pthread_rwlock_unlock(&rwlock);
        break;
    }
    case (SyncMethod::LOCK): {
        pthread_mutex_lock(&mutexlock); // lock for reading
        var = (*gbl_counter);
        pthread_mutex_unlock(&mutexlock);
        break;
    }
    case (SyncMethod::ATOMIC): {
        var = gbl_counter_atomic.load();
        break;
    }
    case (SyncMethod::RACE): {
        var = (*gbl_counter);
        break;
    }
    default:
        throw std::runtime_error("Not implemented!");
    }
    if (sync_method != SyncMethod::RACE) // not guaranteed w/ race conditions
        assert(old_val <= var);          // since gbl_counter is bumping positively always
}

inline void op_finalize()
{
    if (sync_method == SyncMethod::ATOMIC)
    {
        (*gbl_counter) = gbl_counter_atomic.load(); // ensure the global atomic is used as the final "count"
    }
}