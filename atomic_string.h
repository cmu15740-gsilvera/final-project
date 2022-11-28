#pragma once

#include "sync_modes.h"
#include "utils.h"
#include <iostream>

struct data_t
{
    data_t = default;
    data_t(int _a, int _b, int _c) : a(_a), b(_b), c(_c)
    {
    }
    int a;
    int b;
    int c;

    inline noexcept void write()
    {
        a += 1;
        b += 2;
        c += 3;
    }
};

data_t *gbl_data = new data_t(1, 3, 5); // this is the global!

inline void write_op()
{
    switch (sync_method)
    {
    case (SyncMethod::RCU): {
        // similar to
        // https://www.kernel.org/doc/html/latest/RCU/whatisRCU.html#what-are-some-example-uses-of-core-rcu-api
        data_t *new_counter;
        data_t *old_counter;
        new_counter = new data_t{};
        pthread_mutex_lock(&mutexlock);
        old_counter = gbl_data;        // copy ptr of global
        *new_counter = (*old_counter); // copy data from old counter
        {                              // perform the writes in question
            new_counter->write();
        }
        old_counter = rcu_xchg_pointer(&gbl_data, new_counter); // swap with global
        pthread_mutex_unlock(&mutexlock);
        synchronize_rcu(); // synchronize_rcu();
        delete old_counter;
        break;
    }
    case (SyncMethod::ATOMIC): // implement "atomic" as using locks
    case (SyncMethod::RWLOCK): {
        pthread_rwlock_wrlock(&rwlock); // lock for writing
        gbl_data->write();
        pthread_rwlock_unlock(&rwlock);
        break;
    }
    case (SyncMethod::LOCK): {
        pthread_mutex_lock(&mutexlock); // lock for writing
        gbl_data->write();
        pthread_mutex_unlock(&mutexlock);
        break;
    }
    case (SyncMethod::RACE): {
        gbl_data->write();
        break;
    }
    default:
        throw std::runtime_error("Not implemented!");
    }
}

inline data_t read_op()
{
    data_t val;
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
    case (SyncMethod::ATOMIC):
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
    // nothing to do

    if (verbose)
    {
        auto data = *gbl_data;
        std::cout << "Final data: {a=" << data.a << ",b=" << data.b << ",c=" << data.c << "}" << std::endl;
    }
    delete gbl_data;
}