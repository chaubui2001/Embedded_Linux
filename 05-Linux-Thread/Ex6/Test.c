#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define ARRAY_SIZE 1000000
#define NUM_THREADS 4

/* sum and mutex */
long long global_sum = 0;
pthread_mutex_t sum_mutex;

/* Thread data */
typedef struct {
    int *array;
    int start_index;
    int end_index;
} thread_data_t;

/* Thread function to calculate partial sum */
void *calculate_partial_sum(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    long long local_sum = 0;

    /* Calculate the sum of the assigned portion of the array */
    for (int i = data->start_index; i < data->end_index; i++) {
        local_sum += data->array[i];
    }

    /* Print the partial sum */
    printf("Partial sum for range [%d, %d): %lld\n", data->start_index, data->end_index, local_sum);

    /* Lock the mutex before updating the global sum */
    pthread_mutex_lock(&sum_mutex);
    global_sum += local_sum;
    /* Unlock the mutex */
    pthread_mutex_unlock(&sum_mutex);

    pthread_exit(NULL);
}

int main() {
    int *numbers;
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    int ret;
    int chunk_size;

    /* Allocate memory for the array */
    numbers = (int *)malloc(ARRAY_SIZE * sizeof(int));
    if (numbers == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    /* Initialize the array */
    for (int i = 0; i < ARRAY_SIZE; i++) {
        numbers[i] = i + 1;
    }

    /* Calculate the chunk size for each thread */
    chunk_size = ARRAY_SIZE / NUM_THREADS;

    /* Initialize the mutex */
    ret = pthread_mutex_init(&sum_mutex, NULL);
    if (ret != 0) {
        perror("pthread_mutex_init failed");
        exit(EXIT_FAILURE);
    }

    /* Create threads and assign work */
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].array = numbers;
        thread_data[i].start_index = i * chunk_size;
        thread_data[i].end_index = (i == NUM_THREADS - 1) ? ARRAY_SIZE : (i + 1) * chunk_size; // Last thread takes remaining elements

        ret = pthread_create(&threads[i], NULL, calculate_partial_sum, &thread_data[i]);
        if (ret != 0) {
            perror("pthread_create failed");
            exit(EXIT_FAILURE);
        }
    }

    /* Wait for all threads to complete */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    /* Destroy the mutex */
    pthread_mutex_destroy(&sum_mutex);

    /* Print the final sum */
    printf("Total sum: %lld\n", global_sum);

    /* Free allocated memory */
    free(numbers);

    return 0;
}