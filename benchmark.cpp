#include <atomic>    // std::atomic
#include <iomanip>   // std::setprecision
#include <iostream>  // cout
#include <pthread.h> // pthread, mutex
#include <time.h>    // timeval
#include <unistd.h>  // usleep
#include <vector>    // std::vector

// userspace-RCU include
#define _LGPL_SOURCE
// https://github.com/urcu/userspace-rcu#usage-of-liburcu-qsbr
#include <urcu-qsbr.h> // fastest for reads (slightly more intrusive in code)

size_t num_readers; // set by user cmd options
size_t num_writers; // set by user cmd options

size_t RD_OUTER_LOOP = 2000U;
size_t RD_INNER_LOOP = 100000U;

size_t WR_OUTER_LOOP = 10U;
size_t WR_INNER_LOOP = 200U;

typedef size_t counter_t;
counter_t *gbl_counter = new counter_t(0); // this is the global!

bool run_benchmark = false; // used as a barrier flag to start all threads at once (on true)
bool verbose = true;        // disable with 4th optional param

std::atomic<counter_t> gbl_counter_atomic{0};
pthread_rwlock_t rwlock;     // reader-writer lock (supports concurrent readers)
pthread_mutex_t mutexlock;   // single user (reader or writer) mutex
pthread_mutex_t stdout_lock; // stdout_lock is just for pretty printing to stdout (cout)

#define cout_lock(x)                                                                                                   \
    pthread_mutex_lock(&stdout_lock);                                                                                  \
    if (verbose)                                                                                                       \
        std::cout << x << std::endl;                                                                                   \
    pthread_mutex_unlock(&stdout_lock);

typedef uint64_t cycles_t;
static inline cycles_t get_cycles()
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts))
        return -1ULL;
    // assuming nanoseconds ~ cycles (approximately true)
    return ((uint64_t)ts.tv_sec * 1000000000ULL) + ts.tv_nsec;
}

enum SyncMethod : uint8_t
{
    RCU = 0, // uses RCU
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

struct ThreadData
{
    pthread_t thread;
    counter_t counter = 0;
    size_t id = 0;
    cycles_t cycles = 0;
};

std::vector<ThreadData> readers;
std::vector<ThreadData> writers;

inline void update_counter()
{
    switch (sync_method)
    {
    case (SyncMethod::RCU): {
        // similar to
        // https://www.kernel.org/doc/html/latest/RCU/whatisRCU.html#what-are-some-example-uses-of-core-rcu-api
        counter_t *new_counter;
        counter_t *old_counter;
        new_counter = new counter_t{0};
        pthread_mutex_lock(&mutexlock);
        old_counter = gbl_counter;                                 // copy ptr of global
        *new_counter = (*old_counter + 1);                         // bump global's value to local new
        old_counter = rcu_xchg_pointer(&gbl_counter, new_counter); // swap with global
        pthread_mutex_unlock(&mutexlock);
        synchronize_rcu(); // synchronize_rcu();
        delete old_counter;
        break;
    }
    case (SyncMethod::LOCK): {
        pthread_rwlock_wrlock(&rwlock); // lock for writing
        (*gbl_counter)++;               // bump
        pthread_rwlock_unlock(&rwlock);
        break;
    }
    case (SyncMethod::ATOMIC): {
        gbl_counter_atomic++; // bump
        break;
    }
    case (SyncMethod::RACE): {
        (*gbl_counter)++; // bump
        break;
    }
    default:
        throw std::runtime_error("Not implemented!");
    }
}

inline void read_counter(counter_t &var)
{
    counter_t old_val = var;
    switch (sync_method)
    {
    case (SyncMethod::RCU): {
        _rcu_read_lock();
        counter_t *local_ptr = nullptr;
        local_ptr = _rcu_dereference(gbl_counter);
        if (local_ptr)
            var = (*local_ptr);
        _rcu_read_unlock();
        break;
    }
    case (SyncMethod::LOCK): {
        pthread_rwlock_rdlock(&rwlock); // lock for reading
        var = (*gbl_counter);
        pthread_rwlock_unlock(&rwlock);
        break;
    }
    case (SyncMethod::ATOMIC): {
        var = gbl_counter_atomic.load();
        break;
    }
    case (SyncMethod::RACE): {
        var = (*gbl_counter);
        break;
    }
    default:
        throw std::runtime_error("Not implemented!");
    }
    assert(old_val <= var); // since gbl_counter is bumping positively always
}

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
    for (size_t i = 0; i < WR_OUTER_LOOP; i++)
    {
        for (size_t j = 0; j < WR_INNER_LOOP; j++)
        {
            auto t0_ns = get_cycles();
            update_counter();
            auto t1_ns = get_cycles();
            writer.cycles += (t1_ns - t0_ns); // don't account the usleep usec
            usleep(1);                        // sleep for this many microseconds
        }
    }

    if (using_rcu())
        rcu_unregister_thread();

    cout_lock("Finish w(" << id << ") @ " << writer.cycles / 1e9 << "s");
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
            read_counter(reader.counter); // read global counter
        }
        _rcu_quiescent_state();
    }
    auto t1_ns = get_cycles();
    reader.cycles = (t1_ns - t0_ns);

    if (using_rcu())
        rcu_unregister_thread();

    cout_lock("Finish r(" << id << ") @ " << reader.cycles / 1e9 << "s w/ " << reader.counter);
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc < 8)
    {
        std::cout << "Usage: {num_readers} {num_writers} [0(RCU)|1(LOCK)|2(ATOMIC)|3(RACE)] {RD_OUTER_LOOP} "
                     "{RD_INNER_LOOP} {WR_OUTER_LOOP} {WR_INNER_LOOP}"
                  << std::endl;
        exit(1);
    }
    num_readers = std::atoi(argv[1]);
    num_writers = std::atoi(argv[2]);
    sync_method = (SyncMethod)(std::atoi(argv[3]));
    RD_OUTER_LOOP = std::atoi(argv[4]);
    RD_INNER_LOOP = std::atoi(argv[5]);
    WR_OUTER_LOOP = std::atoi(argv[6]);
    WR_INNER_LOOP = std::atoi(argv[7]);

    if (argc == 9) // optional param
        verbose = false;

    if (verbose)
    {
        std::cout << "Reader threads running " << RD_OUTER_LOOP << " outer loops of " << RD_INNER_LOOP << " reads"
                  << std::endl;
        std::cout << "Writer threads running " << WR_OUTER_LOOP << " outer loops of " << WR_INNER_LOOP << " writes"
                  << std::endl;
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

    // join writers
    cycles_t tot_write_cycles = 0;
    for (auto &writer : writers)
    {
        pthread_join(writer.thread, NULL);
        tot_write_cycles += writer.cycles;
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
        const size_t WRITE_LOOP = WR_INNER_LOOP * WR_OUTER_LOOP;
        float cycles_per_write = tot_write_cycles / static_cast<float>(writers.size() * WRITE_LOOP);
        if (verbose)
            std::cout << std::fixed << std::setprecision(3) << "Write -- Avg time: " << tot_write_time / writers.size()
                      << "s | Cycles per write: " << cycles_per_write << std::endl;
        else
            std::cout << cycles_per_write << std::endl;
    }

    if (sync_method == SyncMethod::ATOMIC)
    {
        (*gbl_counter) = gbl_counter_atomic.load(); // ensure the global atomic is used as the final "count"
    }

    if (verbose)
        std::cout << "Final counter: " << (*gbl_counter) << std::endl;
    pthread_rwlock_destroy(&rwlock);
    pthread_mutex_destroy(&mutexlock);
    pthread_mutex_destroy(&stdout_lock);
    delete gbl_counter;
    return 0;
}
