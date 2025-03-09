#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_READERS 5
#define NUM_WRITERS 2

#define WRITE_LIMIT 10

int data = 0;
pthread_rwlock_t rwlock;

/** Reader thread function */
void *reader_function(void *arg) {
    int thread_id = *(int *)arg;
    while (1) {
        pthread_rwlock_rdlock(&rwlock);
        printf("Reader %d: Read data = %d\n", thread_id, data);
        if (data >= WRITE_LIMIT) {
            printf("Reader %d: Data limit reached\n", thread_id);
            pthread_rwlock_unlock(&rwlock);
            break;
        }
        pthread_rwlock_unlock(&rwlock);
        usleep(50000); /** Sleep for writting */
    }
    printf("Reader %d: Exiting\n", thread_id);
    pthread_exit(NULL);
}

/** Writer thread function */
void *writer_function(void *arg) {
    int thread_id = *(int *)arg;
    while(1){
        pthread_rwlock_wrlock(&rwlock);
        if (data >= WRITE_LIMIT) {
            printf("Writer %d: Data limit reached\n", thread_id);
            pthread_rwlock_unlock(&rwlock);
            break;
        }
        data++;
        printf("Writer %d: New data = %d\n", thread_id, data);      
        pthread_rwlock_unlock(&rwlock);
        usleep(200000); /** Sleep for reading */
    }
    printf("Writer %d: Exiting\n", thread_id);
    pthread_exit(NULL);
}

int main() {
    pthread_t readers[NUM_READERS], writers[NUM_WRITERS];
    int reader_ids[NUM_READERS], writer_ids[NUM_WRITERS];
    int ret;

    pthread_rwlock_init(&rwlock, NULL);

    /** Create readers */
    for (int i = 0; i < NUM_READERS; i++) {
        reader_ids[i] = i + 1;
        pthread_create(&readers[i], NULL, reader_function, &reader_ids[i]);
    }

    /** Create writers */
    for (int i = 0; i < NUM_WRITERS; i++) {
        writer_ids[i] = i + 1;
        pthread_create(&writers[i], NULL, writer_function, &writer_ids[i]);
    }

    /** Wait for all threads to finish */
    for (int i = 0; i < NUM_READERS; i++) {
        ret = pthread_join(readers[i], NULL);
        if (ret != 0) {
            perror("pthread_join for reader failed");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < NUM_WRITERS; i++) {
        ret = pthread_join(writers[i], NULL);
        if (ret != 0) {
            perror("pthread_join for writer failed");
            exit(EXIT_FAILURE);
        }
    }

    pthread_rwlock_destroy(&rwlock);
    printf("Final data value: %d\n", data);
    return 0;
}