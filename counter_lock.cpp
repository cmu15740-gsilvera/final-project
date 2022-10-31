#include <chrono>   // std::chrono
#include <iostream> // std::cout
#include <thread>   // std::thread, std::mutex
#include <vector>   // std::vector

// userspace-RCU include
#define _LGPL_SOURCE 1
#include <urcu/urcu-memb.h> // This is the preferred version of the library

constexpr size_t num_readers = 10;
// constexpr int num_writers = 1;

int write_frequency_ns = 10;

volatile size_t gbl_counter = 0; // this is the global!
volatile bool running = true;

std::mutex m;

void bump_counter()
{
    std::lock_guard<std::mutex> lock(m);
    gbl_counter++;
    // std::cout << "bump!" << std::endl;
}

void read_counter(volatile size_t &var)
{
    std::lock_guard<std::mutex> lock(m);
    var = gbl_counter;
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
    while (running)
    {
        std::this_thread::sleep_for(std::chrono::nanoseconds(write_frequency_ns));
        bump_counter();
    }
}

void read_behavior(size_t id)
{
    auto start = std::chrono::high_resolution_clock::now();
    while (thread_data[id].ticks < 1 * 1000 * 1000)
    {
        thread_data[id].ticks++;
        // read global counter
        read_counter(thread_data[id].counter);
    }
    auto finish = std::chrono::high_resolution_clock::now();
    auto ms_count = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
    float runtime_sec = ms_count / 1e9; // ns to s
    thread_data[id].runtime = runtime_sec;
}

int main()
{
    srand(0); // seed randomness

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

    return 0;
}
