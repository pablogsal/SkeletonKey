#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define NUM_THREADS 5
#define NUM_ITERATIONS 3

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// Structure to hold thread-specific data
typedef struct
{
    int thread_id;
    int sleep_min;
    int sleep_max;
} thread_data_t;

void*
worker(void* arg)
{
    thread_data_t* data = (thread_data_t*)arg;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Try to get the lock
        printf("Thread %d trying to acquire lock...\n", data->thread_id);
        pthread_mutex_lock(&lock);

        // Critical section
        printf("Thread %d got the lock!\n", data->thread_id);

        // Simulate some work
        int sleep_time = (rand() % (data->sleep_max - data->sleep_min + 1)) + data->sleep_min;
        printf("Thread %d working for %d seconds...\n", data->thread_id, sleep_time);
        sleep(sleep_time);

        // Release the lock
        printf("Thread %d releasing lock\n", data->thread_id);
        pthread_mutex_unlock(&lock);

        // Wait a bit before trying again
        int wait_time = rand() % 3;
        sleep(wait_time);
    }

    return NULL;
}

int
main()
{
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];

    // Initialize random seed
    srand(time(NULL));

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].thread_id = i + 1;
        thread_data[i].sleep_min = 1;  // Minimum work time
        thread_data[i].sleep_max = 4;  // Maximum work time

        if (pthread_create(&threads[i], NULL, worker, &thread_data[i]) != 0) {
            perror("Failed to create thread");
            return 1;
        }
    }

    // Wait for all threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Cleanup
    pthread_mutex_destroy(&lock);

    return 0;
}
