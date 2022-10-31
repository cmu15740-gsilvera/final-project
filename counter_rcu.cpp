#include <pthread.h> // pthread_create
#include <stdbool.h> // bool
#include <stdio.h>
#include <stdlib.h>

// userspace-RCU include
#define _LGPL_SOURCE 1
#include <urcu/map/urcu-memb.h> // rcu_init
#include <urcu/urcu-memb.h>     // This is the preferred version of the library

const size_t num_readers = 10;
// constexpr int num_writers = 1;

int write_frequency_ns = 10;

volatile size_t *gbl_counter = NULL; // this is the global!
volatile bool running = true;
pthread_mutex_t write_mut;
rcu_flavor_struct rcu;

void bump_counter()
{
    // similar to https://www.kernel.org/doc/html/latest/RCU/whatisRCU.html#what-are-some-example-uses-of-core-rcu-api
    // volatile size_t *new_counter;
    // volatile size_t *old_counter;
    // new_counter = (volatile size_t *)malloc(sizeof(volatile size_t));
    // pthread_mutex_lock(&write_mut);
    // old_counter = rcu_dereference(gbl_counter);
    // *new_counter = (*old_counter + 1); // bump
    // pthread_mutex_unlock(&write_mut);
    // rcu_assign_pointer(gbl_counter, new_counter);
    // urcu_memb_synchronize_rcu(); // synchronize_rcu();
    // free((void *)old_counter);
    (*gbl_counter)++;
}

void read_counter(volatile size_t &var)
{
    // urcu_memb_read_lock();
    // var = *rcu_dereference(gbl_counter);
    // urcu_memb_read_unlock();
    if (gbl_counter != NULL)
    {
        var = *gbl_counter;
    }
}

struct ThreadData
{
    pthread_t thread;
    size_t id = 0;
    volatile size_t counter = 0;
    size_t ticks = 0;
    float runtime = 0.f;
};

ThreadData *thread_data = NULL;

void *write_behavior(void *args)
{
    // urcu_memb_register_thread();
    while (running)
    {
        bump_counter();
        // volatile int x = 0;
        // for (int i = 0; i < 1000; i++)
        // {
        //     x++;
        // }
    }
    // urcu_memb_unregister_thread();
    return NULL;
}

void *read_behavior(void *args)
{
    size_t id = *(size_t *)args;
    // urcu_memb_register_thread();
    if (id >= num_readers)
    {
        printf("Ouf of bounds! Id: %zu", id);
        return NULL;
    }
    // printf("Starting thread %zu\n", id);
    while (thread_data[id].ticks < 10 * 1000 * 1000)
    {
        thread_data[id].ticks++;
        // read global counter
        read_counter(thread_data[id].counter);
    }
    thread_data[id].runtime = 5.6;
    // urcu_memb_unregister_thread();
    return NULL;
}

int main()
{
    gbl_counter = (volatile size_t *)malloc(sizeof(size_t));
    (*gbl_counter) = 0;
    srand(0); // seed randomness
    // urcu_memb_init(); // rcu_init();
    // pthread_mutex_init(&write_mut, NULL);

    pthread_t writer;
    if (pthread_create(&writer, NULL, write_behavior, NULL) != 0)
    {
        printf("Unable to create new writer thread\n");
        exit(1);
    }
    thread_data = (ThreadData *)malloc(sizeof(ThreadData) * num_readers);

    for (size_t i = 0; i < num_readers; i++)
    {
        size_t *args = (size_t *)malloc(sizeof(size_t) * 1);
        (*args) = i;
        if (pthread_create(&(thread_data[i].thread), NULL, read_behavior, (void *)args) != 0)
        {
            printf("Unable to create new thread (%zu)\n", i);
            exit(1);
        }
    }

    // let it run for a while ...

    for (size_t id = 0; id < num_readers; id++)
    {
        (void)pthread_join(thread_data[id].thread, NULL);
    }
    running = false;
    (void)pthread_join(writer, NULL);

    // print final values
    for (size_t id = 0; id < num_readers; id++)
    {
        auto &td = thread_data[id];
        printf("thread %zu: %zu -- %.3fs -- %zu\n", id, td.ticks, td.runtime, td.counter);
    }
    // final counter should be >= any of the threads' local counters in race mode

    printf("Final counter: %zu\n", *gbl_counter);
    free((void *)gbl_counter); // causes segfault??
    free((void *)thread_data);
    // pthread_mutex_destroy(&write_mut);

    return 0;
}
