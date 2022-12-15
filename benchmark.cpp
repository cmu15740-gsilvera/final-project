#include "sync_modes.h" // SyncMode enum
#include "utils.h"      // utils

// what our ops do
#if defined(OP_BUMP_COUNTER)
#include "operations/bump_counter.h"
#elif defined(OP_ATOMIC_STR)
#include "operations/atomic_string.h"
#elif defined(OP_STRUCT_ABC)
#include "operations/struct_abc.h"
#elif defined(OP_ATOMIC_VEC)
#include "operations/atomic_vector.h"
#else
#error("No operation implementations available!")
#endif

#include <iomanip>   // std::setprecision
#include <iostream>  // cout
#include <pthread.h> // pthread, mutex
#include <unistd.h>  // usleep
#include <vector>    // std::vector

size_t num_readers; // set by user cmd options
size_t num_writers; // set by user cmd options

size_t RD_OUTER_LOOP = 2000U;
size_t RD_INNER_LOOP = 100000U;

bool run_benchmark = false;  // used as a barrier flag to start all threads at once (on true)
bool readers_running = true; // used to keep writers running while readers reading
int write_freq_us = 10;      // write frequency in microseconds (us) (1000x ns)

struct ThreadData
{
    pthread_t thread;
    size_t id = 0;
    size_t num_writes = 0;
    size_t num_reads = 0;
    cycles_t cycles = 0;
};

std::vector<ThreadData> readers;
std::vector<ThreadData> writers;

void *write_behavior(void *args)
{
    size_t id = *(size_t *)args;
    if (id >= num_writers)
    {
        std::cerr << "Thread (writer) out of bounds! Id: " << id << std::endl;
        exit(1);
    }

    while (!run_benchmark) // wait until run_benchmark == true to start all threads at once
        usleep(1);

    cout_lock("Begin writer thread " << id);
    if (using_rcu())
        rcu_register_thread();

    auto &writer = writers[id];
    while (readers_running)
    {
        auto t0_ns = get_cycles();
        write_op();
        auto t1_ns = get_cycles();
        writer.cycles += (t1_ns - t0_ns); // don't account the usleep usec
        writer.num_writes++;              // number of writes this thread has committed
        usleep(write_freq_us);            // sleep for this many microseconds
    }

    if (using_rcu())
        rcu_unregister_thread();

    cout_lock("Finish w(" << id << ") @ " << writer.cycles / 1e9 << "s w/ " << writer.num_writes << " writes");
    return NULL;
}

void *read_behavior(void *args)
{
    size_t id = *(size_t *)args;
    if (id >= num_readers)
    {
        std::cerr << "Thread (reader) out of bounds! Id: " << id << std::endl;
        exit(1);
    }
    cout_lock("Begin reader thread " << id);

    while (!run_benchmark) // wait until run_benchmark == true to start all threads at once
        usleep(1);

    auto t0_ns = get_cycles();
    if (using_rcu())
        rcu_register_thread();

    auto &reader = readers[id];

    for (size_t i = 0; i < RD_OUTER_LOOP; i++)
    {
        for (size_t j = 0; j < RD_INNER_LOOP; j++)
        {
            read_op(); // read global counter
            reader.num_reads++;
        }
        _rcu_quiescent_state();
    }
    auto t1_ns = get_cycles();
    reader.cycles = (t1_ns - t0_ns);

    if (using_rcu())
        rcu_unregister_thread();

    cout_lock("Finish r(" << id << ") @ " << reader.cycles / 1e9 << "s w/ " << reader.num_reads << " reads");
    return NULL;
}

enum CMD_PARAMS : uint8_t
{
    _BINARY = 0, // first cmd is the binary name always
    NUM_READERS,
    NUM_WRITERS,
    SYNC_TYPE,
    LOOP_COUNT_OUTER,
    LOOP_COUNT_INNER,

    _SIZE // meta "param" for how many cmd params we have
};

int main(int argc, char **argv)
{
    if (argc < CMD_PARAMS::_SIZE) // required params
    {
        std::cout << "Usage: {num_readers} {num_writers} ";
        std::cout << "[\"RCU\"|\"RWLOCK\"|\"LOCK\"|\"ATOMIC\"|\"RACE\"] ";
        std::cout << "{RD_OUTER_LOOP} {RD_INNER_LOOP} [optional: verbose?]" << std::endl;
        exit(1);
    }
    num_readers = std::atoi(argv[CMD_PARAMS::NUM_READERS]);
    num_writers = std::atoi(argv[CMD_PARAMS::NUM_WRITERS]);
    get_sync_mode(std::string{argv[CMD_PARAMS::SYNC_TYPE]});
    RD_OUTER_LOOP = std::atoi(argv[CMD_PARAMS::LOOP_COUNT_OUTER]);
    RD_INNER_LOOP = std::atoi(argv[CMD_PARAMS::LOOP_COUNT_INNER]);

    if (argc == CMD_PARAMS::_SIZE + 1)
    { // optional param
        verbose = false;
    }

    if (verbose)
    {
        std::cout << "Reader threads running " << RD_OUTER_LOOP << " outer loops of " << RD_INNER_LOOP << " reads"
                  << std::endl;
        std::cout << "Writer threads running with frequency of " << write_freq_us << "us writes" << std::endl;
        std::cout << "Running with " << num_readers << " readers & " << num_writers << " writers" << std::endl;
        std::cout << "Synchronization method: " << SyncName(sync_method) << std::endl << std::endl;
    }

    pthread_rwlock_init(&rwlock, NULL);
    pthread_mutex_init(&mutexlock, NULL);
    pthread_mutex_init(&stdout_lock, NULL);

    // allocate writer threads elements
    writers.reserve(num_writers);
    for (size_t i = 0; i < num_writers; i++)
    {
        size_t *args = new size_t(i);
        writers.push_back(ThreadData());
        if (pthread_create(&(writers[i].thread), NULL, write_behavior, (void *)args) != 0)
        {
            std::cout << "Unable to create new writer thread (" << i << ")" << std::endl;
            exit(1);
        }
    }

    // allocate reader threads elements
    readers.reserve(num_readers);
    for (size_t i = 0; i < num_readers; i++)
    {
        size_t *args = new size_t(i);
        readers.push_back(ThreadData());
        if (pthread_create(&(readers[i].thread), NULL, read_behavior, (void *)args) != 0)
        {
            std::cout << "Unable to create new thread (" << i << ")" << std::endl;
            exit(1);
        }
    }

    run_benchmark = true; // start all the threads at once!
    // let it run for a while ...

    // join readers
    cycles_t tot_read_cycles = 0;
    for (auto &reader : readers)
    {
        pthread_join(reader.thread, NULL);
        tot_read_cycles += reader.cycles;
    }
    readers_running = false; // stop the writers

    // join writers
    cycles_t tot_write_cycles = 0;
    size_t NUM_WRITES = 0;
    for (auto &writer : writers)
    {
        pthread_join(writer.thread, NULL);
        tot_write_cycles += writer.cycles;
        NUM_WRITES += writer.num_writes;
    }

    if (num_readers > 0)
    {
        float tot_read_time = tot_read_cycles / 1e9;
        const size_t READ_LOOP = RD_INNER_LOOP * RD_OUTER_LOOP;
        float cycles_per_read = tot_read_cycles / static_cast<float>(readers.size() * READ_LOOP);
        if (verbose)
            std::cout << std::fixed << std::setprecision(3) << "Read -- Avg time: " << tot_read_time / readers.size()
                      << "s | Cycles per read: " << cycles_per_read << std::endl;
        else
            std::cout << cycles_per_read << std::endl;
    }
    if (num_writers > 0)
    {
        float tot_write_time = tot_write_cycles / 1e9;
        float cycles_per_write = tot_write_cycles / static_cast<float>(writers.size() * NUM_WRITES);
        if (verbose)
            std::cout << std::fixed << std::setprecision(3) << "Write -- Avg time: " << tot_write_time / writers.size()
                      << "s | Cycles per write: " << cycles_per_write << std::endl;
        else
            std::cout << cycles_per_write << std::endl;
    }

    finalize_op();

    pthread_rwlock_destroy(&rwlock);
    pthread_mutex_destroy(&mutexlock);
    pthread_mutex_destroy(&stdout_lock);
    return 0;
}
