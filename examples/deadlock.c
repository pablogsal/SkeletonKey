#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

pthread_mutex_t mutex_a = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_b = PTHREAD_MUTEX_INITIALIZER;

void*
thread_1(void* arg)
{
    for (int i = 0; i < 1000; i++) {
        printf("Thread 1: %d\n", i);
        // Thread 1: A -> B
        pthread_mutex_lock(&mutex_a);
        pthread_mutex_lock(&mutex_b);

        usleep(10);

        pthread_mutex_unlock(&mutex_b);
        pthread_mutex_unlock(&mutex_a);
    }
    return NULL;
}

void*
thread_2(void* arg)
{
    for (int i = 0; i < 1000; i++) {
        printf("Thread 2: %d\n", i);
        // Thread 2: B -> A
        pthread_mutex_lock(&mutex_b);
        pthread_mutex_lock(&mutex_a);

        usleep(10);

        pthread_mutex_unlock(&mutex_a);
        pthread_mutex_unlock(&mutex_b);
    }
    return NULL;
}

int
main()
{
    pthread_t t1, t2;

    pthread_create(&t1, NULL, thread_1, NULL);
    pthread_create(&t2, NULL, thread_2, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    return 0;
}
