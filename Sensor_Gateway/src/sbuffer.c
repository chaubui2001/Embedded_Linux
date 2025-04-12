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
 * @param buffer A pointer to the pointer of the buffer struct to be initialized.
 * The function will allocate memory for the buffer struct.
 * @return GATEWAY_SUCCESS on success, an error code otherwise.
 */
gateway_error_t sbuffer_init(sbuffer_t **buffer) {
    if (buffer == NULL) {
        return GATEWAY_ERROR_INVALID_ARG;
    }

    /* Allocate memory for the buffer structure */
    *buffer = malloc(sizeof(sbuffer_t));
    if (*buffer == NULL) {
        return GATEWAY_ERROR_NOMEM;
    }

    /* Initialize buffer fields */
    (*buffer)->head = 0;
    (*buffer)->tail = 0;
    (*buffer)->count = 0;
    (*buffer)->shutdown_flag = false;

    /* Initialize mutex */
    if (pthread_mutex_init(&((*buffer)->mutex), NULL) != 0) {
        perror("SBuffer ERROR: Mutex initialization failed");
        free(*buffer); /* Clean up allocated memory */
        *buffer = NULL;
        return THREAD_MUTEX_INIT_ERR;
    }

    /* Initialize condition variables */
    if (pthread_cond_init(&((*buffer)->not_full), NULL) != 0) {
        perror("SBuffer ERROR: 'not_full' condition variable initialization failed");
        pthread_mutex_destroy(&((*buffer)->mutex)); /* Clean up mutex */
        free(*buffer);
        *buffer = NULL;
        return THREAD_COND_INIT_ERR;
    }
    if (pthread_cond_init(&((*buffer)->not_empty), NULL) != 0) {
        perror("SBuffer ERROR: 'not_empty' condition variable initialization failed");
        pthread_cond_destroy(&((*buffer)->not_full)); /* Clean up previous cond var */
        pthread_mutex_destroy(&((*buffer)->mutex));
        free(*buffer);
        *buffer = NULL;
        return THREAD_COND_INIT_ERR;
    }

    return GATEWAY_SUCCESS;
}

/**
 * @brief Deallocates the shared buffer and destroys associated mutex and condition variables.
 * @param buffer A pointer to the pointer of the buffer struct to be freed.
 * The pointer *buffer will be set to NULL on success.
 * @return GATEWAY_SUCCESS on success, an error code otherwise.
 */
gateway_error_t sbuffer_free(sbuffer_t **buffer) {
    gateway_error_t result = GATEWAY_SUCCESS;

    if (buffer == NULL || *buffer == NULL) {
        return GATEWAY_ERROR_INVALID_ARG; /* Or GATEWAY_SUCCESS if freeing NULL is okay */
    }

    /* Destroy condition variables */
    if (pthread_cond_destroy(&((*buffer)->not_empty)) != 0) {
        perror("SBuffer WARN: Failed to destroy 'not_empty' condition variable");
        result = SBUFFER_FREE_ERR; /* Report error but continue cleanup */
    }
    if (pthread_cond_destroy(&((*buffer)->not_full)) != 0) {
        perror("SBuffer WARN: Failed to destroy 'not_full' condition variable");
        result = SBUFFER_FREE_ERR;
    }

    /* Destroy mutex */
    if (pthread_mutex_destroy(&((*buffer)->mutex)) != 0) {
        perror("SBuffer WARN: Failed to destroy mutex");
        result = SBUFFER_FREE_ERR;
    }

    /* Free the buffer structure itself */
    free(*buffer);
    *buffer = NULL; /* Set user's pointer to NULL */

    return result;
}

/**
 * @brief Inserts sensor data into the shared buffer (Producer).
 * Blocks if the buffer is full until space becomes available.
 * This function is thread-safe.
 * @param buffer A pointer to the initialized shared buffer.
 * @param data A pointer to the sensor_data_t element to insert.
 * @return GATEWAY_SUCCESS on success, an error code otherwise.
 */
gateway_error_t sbuffer_insert(sbuffer_t *buffer, const sensor_data_t *data) {
    if (buffer == NULL || data == NULL) {
        return GATEWAY_ERROR_INVALID_ARG;
    }

    /* Lock the mutex */
    if (pthread_mutex_lock(&(buffer->mutex)) != 0) {
        perror("SBuffer CRITICAL: Failed to lock mutex in insert");
        return THREAD_MUTEX_LOCK_ERR;
    }

    /* Check shutdown BEFORE waiting */
    if (buffer->shutdown_flag) {
        pthread_mutex_unlock(&(buffer->mutex));
        return SBUFFER_SHUTDOWN;
    }

    /* Wait while the buffer is full */
    while (buffer->count >= SBUFFER_SIZE) {
        /* Wait unlocks mutex, waits, and re-locks upon wakeup */
        if (pthread_cond_wait(&(buffer->not_full), &(buffer->mutex)) != 0) {
            perror("SBuffer CRITICAL: Failed to wait on 'not_full' condition");
            pthread_mutex_unlock(&(buffer->mutex)); /* Attempt to unlock before returning */
            return THREAD_COND_WAIT_ERR;
        }
        /* Re-check condition upon wakeup (due to potential spurious wakeups) */
    }

    /* --- Critical Section --- */
    /* Buffer is not full, insert the data */
    buffer->buffer[buffer->head] = *data; /* Copy data */
    buffer->head = (buffer->head + 1) % SBUFFER_SIZE; /* Move head circularly */
    buffer->count++;
    /* --- End Critical Section --- */

    /* Signal that the buffer is no longer empty (wake up one waiting consumer) */
    if (pthread_cond_signal(&(buffer->not_empty)) != 0) {
        perror("SBuffer WARN: Failed to signal 'not_empty' condition");
        /* Continue anyway, but log the issue */
    }

    /* Unlock the mutex */
    if (pthread_mutex_unlock(&(buffer->mutex)) != 0) {
        perror("SBuffer CRITICAL: Failed to unlock mutex in insert");
        /* Mutex state is uncertain, potential issues */
        return THREAD_MUTEX_UNLOCK_ERR;
    }

    return GATEWAY_SUCCESS;
}

/**
 * @brief Removes sensor data from the shared buffer (Consumer).
 * Blocks if the buffer is empty until data becomes available.
 * This function is thread-safe.
 * @param buffer A pointer to the initialized shared buffer.
 * @param data A pointer to a sensor_data_t struct where the removed data will be copied.
 * @return GATEWAY_SUCCESS on success, an error code otherwise.
 */
gateway_error_t sbuffer_remove(sbuffer_t *buffer, sensor_data_t *data) {
     if (buffer == NULL || data == NULL) {
        return GATEWAY_ERROR_INVALID_ARG;
    }

    /* Lock the mutex */
    if (pthread_mutex_lock(&(buffer->mutex)) != 0) {
        perror("SBuffer CRITICAL: Failed to lock mutex in remove");
        return THREAD_MUTEX_LOCK_ERR;
    }

    /* Check shutdown BEFORE waiting, only return if buffer is also empty */
    if (buffer->shutdown_flag && buffer->count <= 0) {
        pthread_mutex_unlock(&(buffer->mutex));
        return SBUFFER_SHUTDOWN;
    }

    /* Wait while the buffer is empty */
    while (buffer->count <= 0) {
        /* Wait unlocks mutex, waits, and re-locks upon wakeup */
        if (pthread_cond_wait(&(buffer->not_empty), &(buffer->mutex)) != 0) {
            perror("SBuffer CRITICAL: Failed to wait on 'not_empty' condition");
            pthread_mutex_unlock(&(buffer->mutex)); /* Attempt to unlock before returning */
            return THREAD_COND_WAIT_ERR;
        }
        /* Re-check condition upon wakeup */
        /* Check if shutdown is requested while waiting */
        if (buffer->shutdown_flag && buffer->count <= 0) {
            /* Buffer is empty and shutdown is requested */
            pthread_mutex_unlock(&(buffer->mutex)); /* Unlock before returning */
            return SBUFFER_SHUTDOWN; 
        }
    }

    /* --- Critical Section --- */
    /* Buffer is not empty, remove the data */
    *data = buffer->buffer[buffer->tail]; /* Copy data out */
    buffer->tail = (buffer->tail + 1) % SBUFFER_SIZE; /* Move tail circularly */
    buffer->count--;
    /* --- End Critical Section --- */

    /* Signal that the buffer is no longer full (wake up one waiting producer) */
    if (pthread_cond_signal(&(buffer->not_full)) != 0) {
        perror("SBuffer WARN: Failed to signal 'not_full' condition");
        /* Continue anyway, but log the issue */
    }

    /* Unlock the mutex */
    if (pthread_mutex_unlock(&(buffer->mutex)) != 0) {
        perror("SBuffer CRITICAL: Failed to unlock mutex in remove");
        /* Mutex state is uncertain, potential issues */
        return THREAD_MUTEX_UNLOCK_ERR;
    }

    return GATEWAY_SUCCESS;
}

/**
 * @brief Signals all threads waiting on the buffer to wake up for shutdown.
 */
void sbuffer_signal_shutdown(sbuffer_t *buffer) {
    if (buffer == NULL) return;

    if (pthread_mutex_lock(&(buffer->mutex)) != 0) {
        perror("SBuffer CRITICAL: Failed to lock mutex in signal_shutdown");
        return; /* Cannot proceed */
    }

    buffer->shutdown_flag = true; // set shutdown flag

    /* Wake up waiting threads */
    pthread_cond_broadcast(&(buffer->not_empty));
    pthread_cond_broadcast(&(buffer->not_full));

    if (pthread_mutex_unlock(&(buffer->mutex)) != 0) {
        perror("SBuffer CRITICAL: Failed to unlock mutex in signal_shutdown");
    }
}