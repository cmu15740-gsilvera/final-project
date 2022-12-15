#pragma once

#include "../sync_modes.h"
#include "../utils.h"
#include <ctime>   // std::time
#include <iomanip> // std::setprecision, std::put_time
#include <iostream>
#include <sstream> // std::ostringstream
#include <vector>

typedef std::vector<int> data_t;
data_t *gbl_data = new data_t(100, 0); // this is the global! (100 zeros)
#define MAX_LEN 30000

inline void write_vector(data_t &out)
{
    assert(out.size() > 0);
    int idx = std::rand() % out.size();
    out[idx]++;                                                            // increment some random index
    if (idx > static_cast<int>(0.9f * out.size()) && out.size() < MAX_LEN) // int the last 10%
    {
        out.push_back(0); // extend the vector by one
    }
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
        new_counter = new data_t{};
        pthread_mutex_lock(&mutexlock);
        old_counter = gbl_data;                                 // copy ptr of global
        *new_counter = (*old_counter);                          // copy data from old counter
        write_vector(*new_counter);                             // perform write
        old_counter = rcu_xchg_pointer(&gbl_data, new_counter); // swap with global
        pthread_mutex_unlock(&mutexlock);
        synchronize_rcu(); // synchronize_rcu();
        delete old_counter;
        break;
    }
    case (SyncMethod::ATOMIC): // no atomic vector, just uses a lock internally
    case (SyncMethod::RWLOCK): {
        pthread_rwlock_wrlock(&rwlock); // lock for writing
        write_vector(*gbl_data);
        pthread_rwlock_unlock(&rwlock);
        break;
    }
    case (SyncMethod::LOCK): {
        pthread_mutex_lock(&mutexlock); // lock for writing
        write_vector(*gbl_data);
        pthread_mutex_unlock(&mutexlock);
        break;
    }
    case (SyncMethod::RACE): {
        write_vector(*gbl_data);
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

    data_t final_data = *gbl_data;

    int sum = 0;
    for (int x : final_data)
    {
        sum += x;
    }

    if (verbose)
        std::cout << "Final data len: " << final_data.size() << " & sum: " << sum << std::endl;
    delete gbl_data;
}