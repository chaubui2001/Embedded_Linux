#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>

/* Global variable: counter */
int counter = 0;

/* Mutex to protect the counter */
pthread_mutex_t counter_mutex;

/* Condition variable to signal when counter reaches the limit */
pthread_cond_t counter_cond;

/* Flag to indicate whether the counter has reached the limit */
bool counter_reached_limit = false;

/* Thread function */
void *thread_function(void *arg) {
    while (true) {
        /* Lock the mutex */
        pthread_mutex_lock(&counter_mutex);

        /* Check if the counter has reached the limit */
        if (counter_reached_limit) {
            /* Unlock the mutex and exit the thread */
            pthread_mutex_unlock(&counter_mutex);
            pthread_exit(NULL);
        }

        /* Increment the counter */
        counter++;

        /* Check if the counter has reached the limit */
        if (counter >= 1000000) {
            counter_reached_limit = true;
            /* Signal the condition variable */
            pthread_cond_broadcast(&counter_cond);
            /*All threads waiting on counter_cond are awakened*/
        }
        /* Unlock the mutex */
        pthread_mutex_unlock(&counter_mutex);
    }
    pthread_exit(NULL); // Should not reach here, but for safety.
}

int main() {
    pthread_t thread1, thread2, thread3;
    int ret;

    /* Initialize the mutex */
    ret = pthread_mutex_init(&counter_mutex, NULL);
    if (ret != 0) {
        perror("pthread_mutex_init failed");
        exit(EXIT_FAILURE);
    }

    /* Initialize the condition variable */
    ret = pthread_cond_init(&counter_cond, NULL);
    if (ret != 0) {
        perror("pthread_cond_init failed");
        exit(EXIT_FAILURE);
    }
    /* Create three threads */
    ret = pthread_create(&thread1, NULL, thread_function, NULL);
    if (ret != 0) {
        perror("pthread_create for thread1 failed");
        exit(EXIT_FAILURE);
    }
    ret = pthread_create(&thread2, NULL, thread_function, NULL);
      if (ret != 0) {
        perror("pthread_create for thread2 failed");
        exit(EXIT_FAILURE);
    }
    ret = pthread_create(&thread3, NULL, thread_function, NULL);
      if (ret != 0) {
        perror("pthread_create for thread3 failed");
        exit(EXIT_FAILURE);
    }

    /* Wait for threads */
    pthread_mutex_lock(&counter_mutex);
    while (!counter_reached_limit) {
         pthread_cond_wait(&counter_cond, &counter_mutex);
         /*
            pthread_cond_wait() atomically unlocks the mutex and waits
            for the condition variable to be signaled.
         */
    }
    pthread_mutex_unlock(&counter_mutex);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);

    /* Destroy the mutex and condition variable */
    pthread_mutex_destroy(&counter_mutex);
    pthread_cond_destroy(&counter_cond);

    /* Print the final value of the counter */
    printf("Final counter value: %d\n", counter);

    return 0;
}