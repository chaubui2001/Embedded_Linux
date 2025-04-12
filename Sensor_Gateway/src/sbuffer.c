#include <stdlib.h>     /* For malloc, free */
#include <stdio.h>      /* For perror */
#include <pthread.h>    /* For mutex and condition variables */
#include <errno.h>      /* For error codes */
#include <string.h>     /* For memcpy */

/* Include project-specific headers */
#include "config.h"     /* For SBUFFER_SIZE */
#include "common.h"     /* For sensor_data_t, gateway_error_t */
#include "sbuffer.h"    /* For sbuffer_t and function declarations */

/* --- Implementation of Shared Buffer Functions --- */

/**
 * @brief Allocates and initializes a shared buffer.
 * 
 * This function allocates memory for the shared buffer structure and initializes
 * its fields, mutex, and condition variables. It ensures the buffer is ready for
 * use by producer and consumer threads.
 * 
 * @param buffer A pointer to the pointer of the buffer struct to be initialized.
 * @return GATEWAY_SUCCESS on success, an error code otherwise.
 */
gateway_error_t sbuffer_init(sbuffer_t **buffer) {
    if (buffer == NULL) {
        return GATEWAY_ERROR_INVALID_ARG; /* Invalid argument error */
    }

    /* Allocate memory for the buffer structure */
    *buffer = malloc(sizeof(sbuffer_t));
    if (*buffer == NULL) {
        return GATEWAY_ERROR_NOMEM; /* Memory allocation error */
    }

    /* Initialize buffer fields */
    (*buffer)->head = 0; /* Initialize head index */
    (*buffer)->tail = 0; /* Initialize tail index */
    (*buffer)->count = 0; /* Initialize count of elements */
    (*buffer)->shutdown_flag = false; /* Initialize shutdown flag */

    /* Initialize mutex */
    if (pthread_mutex_init(&((*buffer)->mutex), NULL) != 0) {
        perror("SBuffer ERROR: Mutex initialization failed");
        free(*buffer); /* Clean up allocated memory */
        *buffer = NULL;
        return THREAD_MUTEX_INIT_ERR; /* Mutex initialization error */
    }

    /* Initialize condition variables */
    if (pthread_cond_init(&((*buffer)->not_full), NULL) != 0) {
        perror("SBuffer ERROR: 'not_full' condition variable initialization failed");
        pthread_mutex_destroy(&((*buffer)->mutex)); /* Clean up mutex */
        free(*buffer);
        *buffer = NULL;
        return THREAD_COND_INIT_ERR; /* Condition variable initialization error */
    }
    if (pthread_cond_init(&((*buffer)->not_empty), NULL) != 0) {
        perror("SBuffer ERROR: 'not_empty' condition variable initialization failed");
        pthread_cond_destroy(&((*buffer)->not_full)); /* Clean up previous condition variable */
        pthread_mutex_destroy(&((*buffer)->mutex));
        free(*buffer);
        *buffer = NULL;
        return THREAD_COND_INIT_ERR; /* Condition variable initialization error */
    }

    return GATEWAY_SUCCESS; /* Successful initialization */
}

/**
 * @brief Deallocates the shared buffer and destroys associated mutex and condition variables.
 * 
 * This function releases all resources associated with the shared buffer, including
 * memory, mutex, and condition variables. It ensures proper cleanup to avoid resource leaks.
 * 
 * @param buffer A pointer to the pointer of the buffer struct to be freed.
 * @return GATEWAY_SUCCESS on success, an error code otherwise.
 */
gateway_error_t sbuffer_free(sbuffer_t **buffer) {
    gateway_error_t result = GATEWAY_SUCCESS; /* Default result */

    if (buffer == NULL || *buffer == NULL) {
        return GATEWAY_ERROR_INVALID_ARG; /* Invalid argument error */
    }

    /* Destroy condition variables */
    if (pthread_cond_destroy(&((*buffer)->not_empty)) != 0) {
        perror("SBuffer WARN: Failed to destroy 'not_empty' condition variable");
        result = SBUFFER_FREE_ERR; /* Report error but continue cleanup */
    }
    if (pthread_cond_destroy(&((*buffer)->not_full)) != 0) {
        perror("SBuffer WARN: Failed to destroy 'not_full' condition variable");
        result = SBUFFER_FREE_ERR; /* Report error but continue cleanup */
    }

    /* Destroy mutex */
    if (pthread_mutex_destroy(&((*buffer)->mutex)) != 0) {
        perror("SBuffer WARN: Failed to destroy mutex");
        result = SBUFFER_FREE_ERR; /* Report error but continue cleanup */
    }

    /* Free the buffer structure itself */
    free(*buffer);
    *buffer = NULL; /* Set user's pointer to NULL */

    return result; /* Return the result of cleanup */
}

/**
 * @brief Inserts sensor data into the shared buffer (Producer).
 * 
 * This function allows a producer thread to insert data into the shared buffer.
 * It blocks if the buffer is full until space becomes available. The function
 * is thread-safe and ensures proper synchronization.
 * 
 * @param buffer A pointer to the initialized shared buffer.
 * @param data A pointer to the sensor_data_t element to insert.
 * @return GATEWAY_SUCCESS on success, an error code otherwise.
 */
gateway_error_t sbuffer_insert(sbuffer_t *buffer, const sensor_data_t *data) {
    if (buffer == NULL || data == NULL) {
        return GATEWAY_ERROR_INVALID_ARG; /* Invalid argument error */
    }

    /* Lock the mutex */
    if (pthread_mutex_lock(&(buffer->mutex)) != 0) {
        perror("SBuffer CRITICAL: Failed to lock mutex in insert");
        return THREAD_MUTEX_LOCK_ERR; /* Mutex lock error */
    }

    /* Check shutdown BEFORE waiting */
    if (buffer->shutdown_flag) {
        pthread_mutex_unlock(&(buffer->mutex));
        return SBUFFER_SHUTDOWN; /* Shutdown in progress */
    }

    /* Wait while the buffer is full */
    while (buffer->count >= SBUFFER_SIZE) {
        if (pthread_cond_wait(&(buffer->not_full), &(buffer->mutex)) != 0) {
            perror("SBuffer CRITICAL: Failed to wait on 'not_full' condition");
            pthread_mutex_unlock(&(buffer->mutex));
            return THREAD_COND_WAIT_ERR; /* Condition wait error */
        }
    }

    /* --- Critical Section --- */
    buffer->buffer[buffer->head] = *data; /* Copy data into buffer */
    buffer->head = (buffer->head + 1) % SBUFFER_SIZE; /* Move head circularly */
    buffer->count++; /* Increment count */
    /* --- End Critical Section --- */

    /* Signal that the buffer is no longer empty */
    if (pthread_cond_signal(&(buffer->not_empty)) != 0) {
        perror("SBuffer WARN: Failed to signal 'not_empty' condition");
    }

    /* Unlock the mutex */
    if (pthread_mutex_unlock(&(buffer->mutex)) != 0) {
        perror("SBuffer CRITICAL: Failed to unlock mutex in insert");
        return THREAD_MUTEX_UNLOCK_ERR; /* Mutex unlock error */
    }

    return GATEWAY_SUCCESS; /* Successful insertion */
}

/**
 * @brief Removes sensor data from the shared buffer (Consumer).
 * 
 * This function allows a consumer thread to remove data from the shared buffer.
 * It blocks if the buffer is empty until data becomes available. The function
 * is thread-safe and ensures proper synchronization.
 * 
 * @param buffer A pointer to the initialized shared buffer.
 * @param data A pointer to a sensor_data_t struct where the removed data will be copied.
 * @return GATEWAY_SUCCESS on success, an error code otherwise.
 */
gateway_error_t sbuffer_remove(sbuffer_t *buffer, sensor_data_t *data) {
    if (buffer == NULL || data == NULL) {
        return GATEWAY_ERROR_INVALID_ARG; /* Invalid argument error */
    }

    /* Lock the mutex */
    if (pthread_mutex_lock(&(buffer->mutex)) != 0) {
        perror("SBuffer CRITICAL: Failed to lock mutex in remove");
        return THREAD_MUTEX_LOCK_ERR; /* Mutex lock error */
    }

    /* Check shutdown BEFORE waiting */
    if (buffer->shutdown_flag && buffer->count <= 0) {
        pthread_mutex_unlock(&(buffer->mutex));
        return SBUFFER_SHUTDOWN; /* Shutdown in progress */
    }

    /* Wait while the buffer is empty */
    while (buffer->count <= 0) {
        if (pthread_cond_wait(&(buffer->not_empty), &(buffer->mutex)) != 0) {
            perror("SBuffer CRITICAL: Failed to wait on 'not_empty' condition");
            pthread_mutex_unlock(&(buffer->mutex));
            return THREAD_COND_WAIT_ERR; /* Condition wait error */
        }
        if (buffer->shutdown_flag && buffer->count <= 0) {
            pthread_mutex_unlock(&(buffer->mutex));
            return SBUFFER_SHUTDOWN; /* Shutdown in progress */
        }
    }

    /* --- Critical Section --- */
    *data = buffer->buffer[buffer->tail]; /* Copy data out of buffer */
    buffer->tail = (buffer->tail + 1) % SBUFFER_SIZE; /* Move tail circularly */
    buffer->count--; /* Decrement count */
    /* --- End Critical Section --- */

    /* Signal that the buffer is no longer full */
    if (pthread_cond_signal(&(buffer->not_full)) != 0) {
        perror("SBuffer WARN: Failed to signal 'not_full' condition");
    }

    /* Unlock the mutex */
    if (pthread_mutex_unlock(&(buffer->mutex)) != 0) {
        perror("SBuffer CRITICAL: Failed to unlock mutex in remove");
        return THREAD_MUTEX_UNLOCK_ERR; /* Mutex unlock error */
    }

    return GATEWAY_SUCCESS; /* Successful removal */
}

/**
 * @brief Signals all threads waiting on the buffer to wake up for shutdown.
 * 
 * This function sets the shutdown flag and wakes up all threads waiting on
 * the buffer's condition variables. It ensures that threads can exit gracefully.
 * 
 * @param buffer A pointer to the initialized shared buffer.
 */
void sbuffer_signal_shutdown(sbuffer_t *buffer) {
    if (buffer == NULL) return; /* Do nothing if buffer is NULL */

    /* Lock the mutex */
    if (pthread_mutex_lock(&(buffer->mutex)) != 0) {
        perror("SBuffer CRITICAL: Failed to lock mutex in signal_shutdown");
        return; /* Cannot proceed */
    }

    buffer->shutdown_flag = true; /* Set shutdown flag */

    /* Wake up waiting threads */
    pthread_cond_broadcast(&(buffer->not_empty)); /* Wake up consumers */
    pthread_cond_broadcast(&(buffer->not_full)); /* Wake up producers */

    /* Unlock the mutex */
    if (pthread_mutex_unlock(&(buffer->mutex)) != 0) {
        perror("SBuffer CRITICAL: Failed to unlock mutex in signal_shutdown");
    }
}