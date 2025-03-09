#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

/* Executed by each thread */
void *thread_function(void *arg) {
    /* Get the thread ID from the passed argument */
    int thread_id = *(int *)arg;
    /* Print thread ID */
    printf("Thread %d: Hello from thread\n", thread_id);
    /* Exit the thread */
    pthread_exit(NULL);
}

int main() {
    pthread_t thread1, thread2;
    int id1 = 1;             /* ID for thread 1 */
    int id2 = 2;             /* ID for thread 2 */
    int ret;                 /* Return values of pthread functions */

    /* Create thread 1 */
    ret = pthread_create(&thread1, NULL, thread_function, &id1);
    if (ret != 0) {
        perror("pthread_create for thread1 failed");
        exit(EXIT_FAILURE);
    }

    /* Create thread 2 */
    ret = pthread_create(&thread2, NULL, thread_function, &id2);
    if (ret != 0) {
        perror("pthread_create for thread2 failed");
        exit(EXIT_FAILURE);
    }

    /* Wait for thread 1 to complete */
    ret = pthread_join(thread1, NULL);
    if (ret != 0) {
        perror("pthread_join for thread1 failed");
        exit(EXIT_FAILURE);
    }

    /* Wait for thread 2 to complete */
    ret = pthread_join(thread2, NULL);
    if (ret != 0) {
        perror("pthread_join for thread2 failed");
        exit(EXIT_FAILURE);
    }

    /* Print a message after all threads have finished */
    printf("All threads finished\n");
    return 0;
}