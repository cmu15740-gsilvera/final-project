#include <chrono>   // std::chrono
#include <iostream> // std::cout
#include <thread>   // std::thread
#include <vector>   // std::vector

// userspace-RCU include
#define _LGPL_SOURCE 1
#include <urcu/map/urcu-memb.h> // rcu_init
#include <urcu/urcu-memb.h>     // This is the preferred version of the library

constexpr size_t num_readers = 10;
// constexpr int num_writers = 1;

int write_frequency_ns = 10;

volatile size_t *gbl_counter = nullptr; // this is the global!
volatile bool running = true;
std::mutex write_mut;
rcu_flavor_struct rcu;

void bump_counter()
{
    // similar to https://www.kernel.org/doc/html/latest/RCU/whatisRCU.html#what-are-some-example-uses-of-core-rcu-api
    volatile size_t *new_counter;
    volatile size_t *old_counter;
    new_counter = new volatile size_t;
    std::lock_guard<std::mutex> lock(write_mut);
    old_counter = rcu_dereference(gbl_counter);
    *new_counter = (*old_counter + 1); // bump
    rcu_assign_pointer(gbl_counter, new_counter);
    rcu.update_synchronize_rcu(); // synchronize_rcu();
    delete old_counter;

    // std::cout << "bump!" << std::endl;
}

void read_counter(volatile size_t &var)
{
    rcu.read_lock();
    var = *rcu_dereference(gbl_counter);
    rcu.read_unlock();
}

struct ThreadData
{
    ThreadData(std::thread &&t, size_t _id) : thread(std::move(t)), id(_id)
    {
        read_counter(counter);
    }
    class std::thread thread;
    size_t id = 0;
    volatile size_t counter = 0;
    size_t ticks = 0;
    float runtime = 0.f;
};
std::vector<ThreadData> thread_data;

void write_behavior()
{
    std::cout << "writer! test " << std::endl;
    urcu_memb_register_thread();
    std::cout << "writer! test 2" << std::endl;
    while (running)
    {
        std::this_thread::sleep_for(std::chrono::nanoseconds(write_frequency_ns));
        bump_counter();
    }
    urcu_memb_unregister_thread();
}

void read_behavior(size_t id)
{
    // std::cout << "0test " << id << std::endl;
    // rcu.register_thread();
    // std::cout << "test " << id << std::endl;

    // auto start = std::chrono::high_resolution_clock::now();
    // while (thread_data[id].ticks < 1 * 1000 * 1000)
    // {
    //     thread_data[id].ticks++;
    //     // read global counter
    //     read_counter(thread_data[id].counter);
    // }
    // auto finish = std::chrono::high_resolution_clock::now();
    // auto ms_count = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
    // float runtime_sec = ms_count / 1e9; // ns to s
    // thread_data[id].runtime = runtime_sec;
    // rcu.unregister_thread();
}

int main()
{
    gbl_counter = new volatile size_t(0);
    srand(0);         // seed randomness
    urcu_memb_init(); // rcu_init();
    std::cout << "test -1" << std::endl;
    std::thread writer = std::thread(write_behavior);
    thread_data.reserve(num_readers);
    for (size_t i = 0; i < num_readers; i++)
    {
        thread_data[i] = ThreadData(std::thread(read_behavior, i), i);
    }

    // let it run for a while ...

    for (size_t id = 0; id < num_readers; id++)
    {
        thread_data[id].thread.join();
    }
    running = false;
    writer.join();

    // print final values
    for (size_t id = 0; id < num_readers; id++)
    {
        auto &td = thread_data[id];
        std::cout << "thread " << id << ": " << td.ticks << " -- " << td.runtime << "s -- " << td.counter << std::endl;
    }
    // final counter should be >= any of the threads' local counters in race mode
    std::cout << "Final counter: " << gbl_counter << std::endl;
    delete gbl_counter;

    return 0;
}
