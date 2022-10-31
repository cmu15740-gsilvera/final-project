#include <algorithm>
#include <future>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <vector>

// userspace-RCU include
#define _LGPL_SOURCE 1
#include <urcu/urcu-memb.h> // This is the preferred version of the library

constexpr size_t num_readers = 10;
// constexpr int num_writers = 1;

float runtime = 1.f;         // second of total runtime
int write_frequency_ms = 10; // /1000 to get seconds

// this is the global!
size_t gbl_counter = 0;
bool running = true;
std::mutex m;

void bump_counter()
{
    gbl_counter++;
}

void read_counter(size_t &var)
{
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
    size_t counter = 0;
    size_t ticks = 0;
};
std::vector<ThreadData> thread_data;

void write_behavior()
{
    while (running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(write_frequency_ms));
        bump_counter();
    }
}

void read_behavior(size_t id)
{
    while (true)
    {
        thread_data[id].ticks++;
        if (thread_data[id].id != id)
        {
            std::cout << "FAIL: " << thread_data[id].id << "!=" << id << std::endl;
        }
        // read global counter
        read_counter(thread_data[id].counter);
        if (thread_data[id].counter >= runtime * (1000 / write_frequency_ms))
        {
            // std::lock_guard<std::mutex> lock(m);
            // std::cout << "thread " << id << " finished!" << std::endl;
            break;
        }
    }
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
        std::cout << "thread " << id << ": " << thread_data[id].ticks << std::endl;
    }

    return 0;
}
