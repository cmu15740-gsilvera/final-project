#pragma once

#include <string>

// userspace-RCU include
#define _LGPL_SOURCE
// https://github.com/urcu/userspace-rcu#usage-of-liburcu-qsbr
#include <urcu-qsbr.h> // fastest for reads (slightly more intrusive in code)

enum SyncMethod : uint8_t
{
    RCU = 0, // uses RCU
    RWLOCK,  // uses pthread_rwlock
    LOCK,    // uses pthread_rwlock
    ATOMIC,  // uses std::atomic
    RACE,    // uses NO synchronization
    // ...
    SIZE, // [META] how many methods do we have?
};
enum SyncMethod sync_method; // global concurrency synchronization control parameter

std::string SyncName(SyncMethod m)
{
    switch (m)
    {
    case SyncMethod::RCU:
        return "RCU";
    case SyncMethod::LOCK:
        return "LOCK";
    case SyncMethod::ATOMIC:
        return "ATOMIC";
    case SyncMethod::RACE:
        return "NONE";
    default:
        return "UNKNOWN";
    }
    return "";
}

inline bool using_rcu()
{
    return sync_method == SyncMethod::RCU;
}

void get_sync_mode(const std::string &arg)
{
    if (arg == "RCU")
        sync_method = SyncMethod::RCU;
    else if (arg == "RWLOCK")
        sync_method = SyncMethod::RWLOCK;
    else if (arg == "LOCK")
        sync_method = SyncMethod::LOCK;
    else if (arg == "ATOMIC")
        sync_method = SyncMethod::ATOMIC;
    else if (arg == "RACE")
        sync_method = SyncMethod::RACE;
    else
        throw std::runtime_error("unable to interpret \"" + arg + "\"");
}