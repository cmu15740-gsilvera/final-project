#pragma once

#include "../sync_modes.h"
#include "../utils.h"
#include <iostream>
#include <atomic> // std::atomic

typedef size_t data_t;
data_t *gbl_data = new data_t(0); // this is the global!

std::atomic<data_t> gbl_data_atomic{0};


{
    global_t *new_data;
    global_t *old_data;
    new_data = new global_t{};
    pthread_mutex_lock(&mutexlock);
    old_data = global;         // read
    (*new_data) = (*old_data); // copy 
    modify(new_data);          // updates 
    old_counter = rcu_xchg_pointer(&global, new_data);
    pthread_mutex_unlock(&mutexlock);
    synchronize_rcu(); // synchronize_rcu();
    delete old_counter;
}

{
    global_t local_copy;
    ...
    _rcu_read_lock();
    global_t *local_ptr = nullptr;
    local_ptr = _rcu_dereference(global);
    if (local_ptr)
        local_copy = (*local_ptr);
    _rcu_read_unlock();
}

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
        old_counter = gbl_data;                                 // copy ptr of global
        *new_counter = (*old_counter + 1);                      // bump global's value to local new
        old_counter = rcu_xchg_pointer(&gbl_data, new_counter); // swap with global
        pthread_mutex_unlock(&mutexlock);
        synchronize_rcu(); // synchronize_rcu();
        delete old_counter;
        break;
    }
    case (SyncMethod::ATOMIC): {
        gbl_data_atomic++; // bump
        break;
    }
    case (SyncMethod::RWLOCK): {
        pthread_rwlock_wrlock(&rwlock); // lock for writing
        (*gbl_data)++;                  // bump
        pthread_rwlock_unlock(&rwlock);
        break;
    }
    case (SyncMethod::LOCK): {
        pthread_mutex_lock(&mutexlock); // lock for writing
        (*gbl_data)++;                  // bump
        pthread_mutex_unlock(&mutexlock);
        break;
    }
    case (SyncMethod::RACE): {
        (*gbl_data)++; // bump
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
        local_ptr = _rcu_dereference(gbl_data);
        if (local_ptr)
            val = (*local_ptr);
        _rcu_read_unlock();
        break;
    }
    case (SyncMethod::ATOMIC): {
        val = gbl_data_atomic.load();
        break;
    }
    case (SyncMethod::RWLOCK): {
        pthread_rwlock_rdlock(&rwlock); // lock for reading
        val = (*gbl_data);
        pthread_rwlock_unlock(&rwlock);
        break;
    }
    case (SyncMethod::LOCK): {
        pthread_mutex_lock(&mutexlock); // lock for reading
        val = (*gbl_data);
        pthread_mutex_unlock(&mutexlock);
        break;
    }
    case (SyncMethod::RACE): {
        val = (*gbl_data);
        break;
    }
    default:
        throw std::runtime_error("Not implemented!");
    }
    return val;
}

inline void finalize_op()
{
    data_t final_data = *gbl_data;
    if (sync_method == SyncMethod::ATOMIC)
    {
        final_data = gbl_data_atomic.load(); // ensure the global atomic is used as the final "count"
    }

    if (verbose)
        std::cout << "Final data: " << final_data << std::endl;

    delete gbl_data;
}