#pragma once

#include "sync_modes.h"
#include "utils.h"
#include <iostream>

typedef size_t data_t;
data_t *gbl_counter = new data_t(0); // this is the global!

std::atomic<data_t> gbl_counter_atomic{0};

inline void write_op()
{
    switch (sync_method)
    {
    case (SyncMethod::RCU): {
        // similar to
        // https://www.kernel.org/doc/html/latest/RCU/whatisRCU.html#what-are-some-example-uses-of-core-rcu-api
        data_t *new_counter;
        data_t *old_counter;
        new_counter = new data_t{0};
        pthread_mutex_lock(&mutexlock);
        old_counter = gbl_counter;                                 // copy ptr of global
        *new_counter = (*old_counter + 1);                         // bump global's value to local new
        old_counter = rcu_xchg_pointer(&gbl_counter, new_counter); // swap with global
        pthread_mutex_unlock(&mutexlock);
        synchronize_rcu(); // synchronize_rcu();
        delete old_counter;
        break;
    }
    case (SyncMethod::ATOMIC): {
        gbl_counter_atomic++; // bump
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
    case (SyncMethod::RACE): {
        (*gbl_counter)++; // bump
        break;
    }
    default:
        throw std::runtime_error("Not implemented!");
    }
}

inline data_t read_op()
{
    data_t val = 0;
    switch (sync_method)
    {
    case (SyncMethod::RCU): {
        _rcu_read_lock();
        data_t *local_ptr = nullptr;
        local_ptr = _rcu_dereference(gbl_counter);
        if (local_ptr)
            val = (*local_ptr);
        _rcu_read_unlock();
        break;
    }
    case (SyncMethod::ATOMIC): {
        val = gbl_counter_atomic.load();
        break;
    }
    case (SyncMethod::RWLOCK): {
        pthread_rwlock_rdlock(&rwlock); // lock for reading
        val = (*gbl_counter);
        pthread_rwlock_unlock(&rwlock);
        break;
    }
    case (SyncMethod::LOCK): {
        pthread_mutex_lock(&mutexlock); // lock for reading
        val = (*gbl_counter);
        pthread_mutex_unlock(&mutexlock);
        break;
    }
    case (SyncMethod::RACE): {
        val = (*gbl_counter);
        break;
    }
    default:
        throw std::runtime_error("Not implemented!");
    }
    return val;
}

inline void finalize_op()
{
    data_t final_count = *gbl_counter;
    if (sync_method == SyncMethod::ATOMIC)
    {
        final_count = gbl_counter_atomic.load(); // ensure the global atomic is used as the final "count"
    }

    if (verbose)
        std::cout << "Final counter: " << final_count << std::endl;

    delete gbl_counter;
}