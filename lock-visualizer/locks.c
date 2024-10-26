#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

pthread_mutex_t locks[3] = {
        PTHREAD_MUTEX_INITIALIZER,
        PTHREAD_MUTEX_INITIALIZER,
        PTHREAD_MUTEX_INITIALIZER};

void*
thread_func(void* arg)
{
    int thread_id = *(int*)arg;

    while (1) {
        // Each thread tries different lock patterns
        switch (thread_id) {
            case 0:  // Thread 0 uses lock 0, then 1
                pthread_mutex_lock(&locks[0]);
                usleep(100000);  // 100ms
                pthread_mutex_lock(&locks[1]);
                usleep(200000);  // 200ms
                pthread_mutex_unlock(&locks[1]);
                pthread_mutex_unlock(&locks[0]);
                break;

            case 1:  // Thread 1 uses lock 1, then 2
                pthread_mutex_lock(&locks[1]);
                usleep(150000);  // 150ms
                pthread_mutex_lock(&locks[2]);
                usleep(100000);  // 100ms
                pthread_mutex_unlock(&locks[2]);
                pthread_mutex_unlock(&locks[1]);
                break;

            case 2:  // Thread 2 uses lock 0, then 2
                pthread_mutex_lock(&locks[0]);
                usleep(200000);  // 200ms
                pthread_mutex_lock(&locks[2]);
                usleep(150000);  // 150ms
                pthread_mutex_unlock(&locks[2]);
                pthread_mutex_unlock(&locks[0]);
                break;

            case 3:  // Thread 3 uses all locks in order
                pthread_mutex_lock(&locks[0]);
                usleep(50000);  // 50ms
                pthread_mutex_lock(&locks[1]);
                usleep(50000);  // 50ms
                pthread_mutex_lock(&locks[2]);
                usleep(100000);  // 100ms
                pthread_mutex_unlock(&locks[2]);
                pthread_mutex_unlock(&locks[1]);
                pthread_mutex_unlock(&locks[0]);
                break;

            case 4:  // Thread 4 alternates between locks 1 and 2
                pthread_mutex_lock(&locks[1]);
                usleep(100000);  // 100ms
                pthread_mutex_unlock(&locks[1]);
                usleep(50000);  // 50ms
                pthread_mutex_lock(&locks[2]);
                usleep(100000);  // 100ms
                pthread_mutex_unlock(&locks[2]);
                break;
        }

        // Small sleep between iterations
        usleep(50000);  // 50ms
    }

    return NULL;
}

int
main()
{
    pthread_t threads[5];
    int thread_ids[5] = {0, 1, 2, 3, 4};

    // Create threads
    for (int i = 0; i < 5; i++) {
        pthread_create(&threads[i], NULL, thread_func, &thread_ids[i]);
    }

    // Let it run for a while
    sleep(5);

    return 0;
}
