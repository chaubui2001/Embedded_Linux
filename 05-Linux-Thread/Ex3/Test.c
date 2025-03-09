#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

/* Shared data */
int data = 0;
bool data_ready = false; /* Indicate if data is ready */

/* Mutex and condition variable */
pthread_mutex_t data_mutex;
pthread_cond_t data_cond;

/* Producer thread function */
void *producer_function(void *arg) {
    for (int i = 0; i < 10; i++) {
        /* Simulate */
        usleep(100000); // Sleep for 100ms (0.1 seconds)

        /* Lock the mutex */
        pthread_mutex_lock(&data_mutex);

        /* Generate a random number between 1 and 10 */
        data = rand() % 10 + 1;
        data_ready = true;

        printf("Producer: Produced data = %d\n", data);

        /* Signal the consumer that data is ready */
        pthread_cond_signal(&data_cond);

        /* Unlock the mutex */
        pthread_mutex_unlock(&data_mutex);
    }
    pthread_exit(NULL);
}

/* Consumer thread function */
void *consumer_function(void *arg) {
    for (int i = 0; i < 10; i++) {
        /* Lock the mutex */
        pthread_mutex_lock(&data_mutex);

        /* Wait until data is ready */
        while (!data_ready) {
            pthread_cond_wait(&data_cond, &data_mutex);
            /*
            - Atomically unlocks the mutex and waits.
            - When signaled, re-acquires the mutex before returning.
            */
        }

        /* Consume the data */
        printf("Consumer: Consumed data = %d\n", data);
        data_ready = false;

        /* Unlock the mutex */
        pthread_mutex_unlock(&data_mutex);
    }
    pthread_exit(NULL);
}

int main() {
    pthread_t producer, consumer;
    int ret;

    /* Initialize mutex and condition variable */
    pthread_mutex_init(&data_mutex, NULL);
    pthread_cond_init(&data_cond, NULL);

    /* Create threads */
    ret = pthread_create(&producer, NULL, producer_function, NULL);
    if (ret != 0) {
        perror("pthread_create for producer failed");
        exit(EXIT_FAILURE);
    }

    ret = pthread_create(&consumer, NULL, consumer_function, NULL);
    if (ret != 0) {
        perror("pthread_create for consumer failed");
        exit(EXIT_FAILURE);
    }

    /* Wait for threads to finish */
    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);

    /* Destroy mutex and condition variable */
    pthread_mutex_destroy(&data_mutex);
    pthread_cond_destroy(&data_cond);

    return 0;
}