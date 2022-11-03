#include <iomanip>   // std::setprecision
#include <iostream>  // cout
#include <pthread.h> // pthread, mutex
#include <time.h>    // timeval
#include <vector>    // std::vector

// userspace-RCU include
#define _LGPL_SOURCE
#if !defined(_LGPL_SOURCE)
// https://gist.github.com/azat/066d165154fa1efe6000fac59062cc25
#error URCU is very slow w/o _LGPL_SOURCE
#endif
#include <urcu/urcu-memb.h> // This is the preferred version of the library

#if defined(_LGBL_SOURCE)
typedef volatile size_t counter_t;
#else
// no volatile for non-_LGPL_SOURCE
typedef size_t counter_t;
#endif

size_t num_readers; // set by user cmd options
size_t num_writers; // set by user cmd options
bool use_rcu;       // set by user cmd options

#define OUTER_READ_LOOP 200U
#define INNER_READ_LOOP 100000U
#define READ_LOOP ((unsigned long long)OUTER_READ_LOOP * INNER_READ_LOOP)

#define OUTER_WRITE_LOOP 10U
#define INNER_WRITE_LOOP 200U
#define WRITE_LOOP ((unsigned long long)OUTER_WRITE_LOOP * INNER_WRITE_LOOP)

counter_t *gbl_counter = new counter_t(0); // this is the global!

pthread_rwlock_t rwlock;     // reader-writer lock (supports concurrent readers)
pthread_mutex_t mutexlock;   // single user (reader or writer) mutex
pthread_mutex_t stdout_lock; // stdout_lock is just for pretty printing to stdout (cout)

typedef uint64_t cycles_t;
static inline cycles_t get_cycles()
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts))
        return -1ULL;
    // assuming nanoseconds ~ cycles (approximately true)
    return ((uint64_t)ts.tv_sec * 1000000000ULL) + ts.tv_nsec;
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
    if (use_rcu)
    {
        // similar to
        // https://www.kernel.org/doc/html/latest/RCU/whatisRCU.html#what-are-some-example-uses-of-core-rcu-api
        counter_t *new_counter;
        counter_t *old_counter;
        new_counter = new counter_t(0);
        pthread_mutex_lock(&mutexlock);
#if defined(_LGPL_SOURCE)
        old_counter = rcu_dereference(gbl_counter);
#else
        old_counter = (counter_t *)rcu_dereference((void *)gbl_counter);
#endif
        *new_counter = (*old_counter + 1); // bump
        rcu_assign_pointer(gbl_counter, new_counter);
        pthread_mutex_unlock(&mutexlock);
        urcu_memb_synchronize_rcu(); // synchronize_rcu();
        delete old_counter;
    }
    else
    {
        pthread_rwlock_wrlock(&rwlock); // lock for writing
        (*gbl_counter)++;               // bump
        pthread_rwlock_unlock(&rwlock);
    }
}

inline void read_counter(counter_t &var)
{
    counter_t old_val = var;
    if (use_rcu)
    {
        urcu_memb_read_lock();
        counter_t *local_ptr = nullptr;
#if defined(_LGPL_SOURCE)
        local_ptr = rcu_dereference(gbl_counter);
#else
        local_ptr = (counter_t *)rcu_dereference((void *)gbl_counter);
#endif
        if (local_ptr)
        {
            var = (*local_ptr);
        }
        urcu_memb_read_unlock();
    }
    else
    {
        pthread_rwlock_rdlock(&rwlock); // lock for reading
        var = (*gbl_counter);
        pthread_rwlock_unlock(&rwlock);
    }
    assert(old_val <= var); // since gbl_counter is bumping positively always
}

#define cout_lock(x)                                                                                                   \
    pthread_mutex_lock(&stdout_lock);                                                                                  \
    std::cout << x << std::endl;                                                                                       \
    pthread_mutex_unlock(&stdout_lock);

void *write_behavior(void *args)
{
    size_t id = *(size_t *)args;
    if (id >= num_writers)
    {
        std::cerr << "Thread (writer) out of bounds! Id: " << id << std::endl;
        exit(1);
    }
    cout_lock("Begin writer thread " << id);
    auto t0_ns = get_cycles();
    if (use_rcu)
        urcu_memb_register_thread();

    for (size_t i = 0; i < OUTER_WRITE_LOOP; i++)
    {
        for (size_t j = 0; j < INNER_WRITE_LOOP; j++)
        {
            update_counter();
            usleep(1); // sleep for this many microseconds
        }
    }
    auto t1_ns = get_cycles();
    writers[id].cycles = (t1_ns - t0_ns);

    if (use_rcu)
        urcu_memb_unregister_thread();

    cout_lock("Finish w(" << id << ") @ " << writers[id].cycles / 1e9 << "s");
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
    auto t0_ns = get_cycles();
    if (use_rcu)
        urcu_memb_register_thread();

    for (size_t i = 0; i < OUTER_READ_LOOP; i++)
    {
        for (size_t j = 0; j < INNER_READ_LOOP; j++)
        {
            read_counter(readers[id].counter); // read global counter
        }
    }
    auto t1_ns = get_cycles();
    readers[id].cycles = (t1_ns - t0_ns);

    if (use_rcu)
        urcu_memb_unregister_thread();

    cout_lock("Finish r(" << id << ") @ " << readers[id].cycles / 1e9 << "s");
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cout << "Usage: ./benchmark.out {num_readers} {num_writers} [y/n]" << std::endl;
        exit(1);
    }
    num_readers = std::atoi(argv[1]);
    num_writers = std::atoi(argv[2]);
    use_rcu = (*argv[3] == 'y');
    std::cout << "Running with " << num_readers << " readers & " << num_writers << " writers" << std::endl;
    auto rcu_enabled = use_rcu ? "ON" : "OFF";
    std::cout << "RCU: " << rcu_enabled << std::endl << std::endl;

    if (use_rcu)
    {
        urcu_memb_init(); // rcu_init();
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

    // let it run for a while ...
    // sleep(3);

    // join readers
    cycles_t tot_read_cycles = 0;
    for (size_t id = 0; id < readers.size(); id++)
    {
        pthread_join(readers[id].thread, NULL);
        tot_read_cycles += readers[id].cycles;
    }

    // join writers
    cycles_t tot_write_cycles = 0;
    for (size_t id = 0; id < writers.size(); id++)
    {
        pthread_join(writers[id].thread, NULL);
        tot_write_cycles += writers[id].cycles;
    }

    if (num_readers > 0)
    {
        float tot_read_time = tot_read_cycles / 1e9;
        cycles_t cycles_per_read = tot_read_cycles / static_cast<float>(readers.size() * READ_LOOP);
        std::cout << std::fixed << std::setprecision(3) << "Read -- Avg time: " << tot_read_time / readers.size()
                  << "s | Cycles per read: " << cycles_per_read << std::endl;
    }
    if (num_writers > 0)
    {
        float tot_write_time = tot_write_cycles / 1e9;
        cycles_t cycles_per_write = tot_write_cycles / static_cast<float>(writers.size() * WRITE_LOOP);
        std::cout << std::fixed << std::setprecision(3) << "Write -- Avg time: " << tot_write_time / readers.size()
                  << "s | Cycles per write: " << cycles_per_write << std::endl;
    }

    std::cout << "Final counter: " << (*gbl_counter) << std::endl;
    pthread_rwlock_destroy(&rwlock);
    pthread_mutex_init(&mutexlock, NULL);
    pthread_mutex_destroy(&stdout_lock);
    delete gbl_counter;
    return 0;
}
