#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define ARRAY_SIZE 100

/* Thread data */
typedef struct {
    int *array;
    int start_index;
    int end_index;
    int count; /* Result: number of even/odd numbers */
} thread_data_t;

/* Thread function to count even numbers */
void *count_even(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    data->count = 0; // Initialize count
    for (int i = data->start_index; i < data->end_index; i++) {
        if (data->array[i] % 2 == 0) {
            data->count++;
        }
    }
    pthread_exit(NULL);
}

/* Thread function to count odd numbers */
void *count_odd(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    data->count = 0;  // Initialize count
     for (int i = data->start_index; i < data->end_index; i++) {
        if (data->array[i] % 2 != 0) {
            data->count++;
        }
    }
    pthread_exit(NULL);
}

int main() {
    int numbers[ARRAY_SIZE];
    pthread_t even_thread, odd_thread;
    thread_data_t even_data, odd_data;
    int ret;

    /* Seed the random number generator */
    srand(time(NULL));

    /* Initialize the array with random numbers (1-100) */
    for (int i = 0; i < ARRAY_SIZE; i++) {
        numbers[i] = rand() % 100 + 1;
    }

    /* Prepare data for the even thread */
    even_data.array = numbers;
    even_data.start_index = 0;
    even_data.end_index = ARRAY_SIZE;
    even_data.count = 0;

    /* Prepare data for the odd thread */
    odd_data.array = numbers;
    odd_data.start_index = 0;
    odd_data.end_index = ARRAY_SIZE;
    odd_data.count = 0;
    
    /* Create threads */
    ret = pthread_create(&even_thread, NULL, count_even, &even_data);
      if (ret != 0) {
        perror("pthread_create for even_thread failed");
        exit(EXIT_FAILURE);
    }
    ret = pthread_create(&odd_thread, NULL, count_odd, &odd_data);
      if (ret != 0) {
        perror("pthread_create for odd_thread failed");
        exit(EXIT_FAILURE);
    }
    /* Wait for threads to finish */
    pthread_join(even_thread, NULL);
    pthread_join(odd_thread, NULL);

    /* Print the results */
    printf("Number of even numbers: %d\n", even_data.count);
    printf("Number of odd numbers: %d\n", odd_data.count);

    return 0;
}