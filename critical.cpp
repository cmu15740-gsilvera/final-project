#include <algorithm>
#include <future>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <vector>

// userspace-RCU include
#define _LGPL_SOURCE 1
#include <urcu/urcu-memb.h> // This is the preferred version of the library

constexpr int num_readers = 10;
// constexpr int num_writers = 1;

const int data_size = 1000 * 1000 * 100;
std::mutex m;
std::vector<float> data;
bool running = true; // global flag to stop write thread on program completion
std::vector<float> answers;

void write_behavior()
{
    // repeatedly swaps two randomly selected elements
    srand(0);
    while (running)
    {
        int random_idx1 = rand() % data_size;
        int random_idx2 = rand() % data_size;
        std::swap(data[random_idx1], data[random_idx2]);
    }
}

void read_behavior(int id)
{
    const int amnt_read = data_size / num_readers;
    int start = id * amnt_read;
    float ret = 0.f;
    for (int i = start; i < start + amnt_read; i++)
    {
        ret += data[i];
    }
    answers[id] = ret / 1000.f;
}

void init_data()
{
    data.resize(data_size);
    srand(0); // seed at 0
    for (int i = 0; i < data_size; i++)
    {
        // push random number from 0..100
        data[i] = rand() % 100;
    }
}

int main()
{
    init_data();

    std::thread writer = std::thread(write_behavior);
    std::vector<std::thread> readers;
    for (int i = 0; i < num_readers; i++)
    {
        readers.push_back(std::thread(read_behavior, i));
        answers.push_back(0.f);
    }

    for (int id = 0; id < num_readers; id++)
    {
        readers[id].join();
        std::cout << "section " << id << ": " << answers[id] << std::endl;
    }
    running = false;
    writer.join();

    return 0;
}
