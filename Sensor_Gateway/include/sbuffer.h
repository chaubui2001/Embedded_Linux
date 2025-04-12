#ifndef SBUFFER_H
#define SBUFFER_H

#include <pthread.h> /* Required for pthread types */
#include "common.h"  /* Required for sensor_data_t and gateway_error_t */
#include "config.h"  /* Required for SBUFFER_SIZE */

/* * Structure definition for the shared buffer (circular buffer).
 * Uses mutex and condition variables for thread safety.
 */
typedef struct {
    sensor_data_t buffer[SBUFFER_SIZE]; /* The actual buffer storage */
    int head;                           /* Index where the next element will be inserted */
    int tail;                           /* Index from where the next element will be removed */
    int count;                          /* Number of elements currently in the buffer */
    pthread_mutex_t mutex;              /* Mutex to protect access to buffer fields */
    pthread_cond_t not_full;            /* Condition variable signaled when buffer is not full */
    pthread_cond_t not_empty;           /* Condition variable signaled when buffer is not empty */
    bool shutdown_flag;                 /* Flag to indicate if shutdown is requested */
} sbuffer_t;

/**
 * Allocates and initializes a shared buffer.
 * @param buffer A pointer to the pointer of the buffer struct to be initialized.
 * The function will allocate memory for the buffer struct.
 * @return GATEWAY_SUCCESS on success, an error code otherwise (e.g., GATEWAY_ERROR_NOMEM, THREAD_MUTEX_INIT_ERR, THREAD_COND_INIT_ERR).
 */
gateway_error_t sbuffer_init(sbuffer_t **buffer);

/**
 * Deallocates the shared buffer and destroys associated mutex and condition variables.
 * @param buffer A pointer to the pointer of the buffer struct to be freed.
 * The pointer *buffer will be set to NULL on success.
 * @return GATEWAY_SUCCESS on success, an error code otherwise (e.g., SBUFFER_FREE_ERR).
 */
gateway_error_t sbuffer_free(sbuffer_t **buffer);

/**
 * Inserts sensor data into the shared buffer.
 * Blocks if the buffer is full until space becomes available.
 * This function is thread-safe.
 * @param buffer A pointer to the initialized shared buffer.
 * @param data A pointer to the sensor_data_t element to insert.
 * @return GATEWAY_SUCCESS on success, an error code otherwise.
 */
gateway_error_t sbuffer_insert(sbuffer_t *buffer, const sensor_data_t *data);

/**
 * Removes sensor data from the shared buffer.
 * Blocks if the buffer is empty until data becomes available.
 * This function is thread-safe.
 * @param buffer A pointer to the initialized shared buffer.
 * @param data A pointer to a sensor_data_t struct where the removed data will be copied.
 * @return GATEWAY_SUCCESS on success, an error code otherwise.
 */
gateway_error_t sbuffer_remove(sbuffer_t *buffer, sensor_data_t *data);

/**
 * @brief Signals all threads waiting on the buffer to wake up for shutdown.
 * Sets an internal flag and broadcasts on condition variables.
 * @param buffer A pointer to the initialized shared buffer.
 */
 void sbuffer_signal_shutdown(sbuffer_t *buffer);

#endif /* SBUFFER_H */