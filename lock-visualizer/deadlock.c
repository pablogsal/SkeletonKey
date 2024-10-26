#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock2 = PTHREAD_MUTEX_INITIALIZER;

void*
thread1_func(void* arg)
{
    printf("Thread 1: Starting\n");
    printf("Thread 1: Acquiring lock1\n");
    pthread_mutex_lock(&lock1);
    printf("Thread 1: Acquired lock1\n");

    // Sleep to make deadlock more likely
    usleep(500000);  // 500ms

    printf("Thread 1: Trying to acquire lock2\n");
    pthread_mutex_lock(&lock2);
    printf("Thread 1: Acquired lock2\n");

    // We'll never get here due to deadlock
    pthread_mutex_unlock(&lock2);
    pthread_mutex_unlock(&lock1);

    return NULL;
}

void*
thread2_func(void* arg)
{
    printf("Thread 2: Starting\n");
    printf("Thread 2: Acquiring lock2\n");
    pthread_mutex_lock(&lock2);
    printf("Thread 2: Acquired lock2\n");

    // Sleep to make deadlock more likely
    usleep(500000);  // 500ms

    printf("Thread 2: Trying to acquire lock1\n");
    pthread_mutex_lock(&lock1);
    printf("Thread 2: Acquired lock1\n");

    // We'll never get here due to deadlock
    pthread_mutex_unlock(&lock1);
    pthread_mutex_unlock(&lock2);

    return NULL;
}

int
main()
{
    pthread_t thread1, thread2;

    // Create threads
    pthread_create(&thread1, NULL, thread1_func, NULL);
    pthread_create(&thread2, NULL, thread2_func, NULL);

    // Wait for threads (they'll never finish due to deadlock)
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    return 0;
}
