#pragma once

#include "../sync_modes.h"
#include "../utils.h"
#include <ctime>   // std::time
#include <iomanip> // std::setprecision, std::put_time
#include <iostream>
#include <sstream> // std::ostringstream
#include <string>

typedef std::string data_t;
data_t *gbl_data = new data_t(""); // this is the global!

inline void write_str(data_t &out)
{
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%d-%m-%Y %H-%M-%S");
    out = oss.str();
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
        write_str(*new_counter);                                // perform write
        old_counter = rcu_xchg_pointer(&gbl_data, new_counter); // swap with global
        pthread_mutex_unlock(&mutexlock);
        synchronize_rcu(); // synchronize_rcu();
        delete old_counter;
        break;
    }
    case (SyncMethod::ATOMIC): // no atomic string
    case (SyncMethod::RWLOCK): {
        pthread_rwlock_wrlock(&rwlock); // lock for writing
        write_str(*gbl_data);
        pthread_rwlock_unlock(&rwlock);
        break;
    }
    case (SyncMethod::LOCK): {
        pthread_mutex_lock(&mutexlock); // lock for writing
        write_str(*gbl_data);
        pthread_mutex_unlock(&mutexlock);
        break;
    }
    case (SyncMethod::RACE): {
        write_str(*gbl_data);
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
    if (verbose)
        std::cout << "Final data: \"" << final_data << "\"" << std::endl;
    delete gbl_data;
}