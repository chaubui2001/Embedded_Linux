#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     /* For sleep() replacement using nanosleep */
#include <sqlite3.h>    /* Needed for sqlite3* type */
#include <pthread.h>    /* Potentially for thread types if used */
#include <stdbool.h>    /* For bool type */
#include <string.h>     /* For strerror, memcpy */
#include <time.h>       /* For time_t, nanosleep, time() */
#include <signal.h>     /* For sig_atomic_t */
#include <errno.h>      /* For errno */

/* Include project-specific headers */
#include "config.h"     /* For DB config constants like DB_NAME, etc. */
#include "common.h"     /* For sensor_data_t, gateway_error_t */
#include "sbuffer.h"    /* For sbuffer functions */
#include "logger.h"     /* For log_message */
#include "db_handler.h" /* For DB interaction functions */
#include "storagemgt.h" /* For function declarations */

/* --- Local Macros --- */

/* Sleep interval in milliseconds for interruptible sleep */
#define SHORT_SLEEP_MS 100

/* Initial capacity for the local retry queue */
#define RETRY_QUEUE_INITIAL_CAPACITY 20

/* --- Local Structures --- */

/**
 * @brief Structure for the local retry queue (simple circular buffer).
 * Used to hold sensor data that failed to be inserted into the DB due to connection issues.
 */
typedef struct {
    sensor_data_t *items;   /* Dynamically allocated array of sensor data items */
    int count;              /* Number of items currently in the queue */
    int capacity;           /* Allocated capacity of the items array */
    int head;               /* Index to read from (oldest item) */
    int tail;               /* Index to write to (newest item) */
} local_retry_queue_t;

/* --- Static Variables --- */

/* The local retry queue instance */
static local_retry_queue_t retry_queue;

/* Flag indicating if the retry queue has been initialized */
static bool queue_initialized = false;

/* --- External Variables --- */

/* Global flag from main.c to signal termination */
extern volatile sig_atomic_t terminate_flag;

/* --- Forward Declarations (Internal Helper Functions) --- */

/* Sleep function that checks terminate_flag periodically */
static void interruptible_sleep(unsigned int seconds);

/* Retry queue management functions */
static gateway_error_t init_retry_queue(int initial_capacity);
static void free_retry_queue(void);
static bool is_retry_queue_empty(void);
static bool is_retry_queue_full(void);
static gateway_error_t enqueue_retry_item(const sensor_data_t *data);
static gateway_error_t dequeue_retry_item(sensor_data_t *data);
static gateway_error_t peek_retry_item(sensor_data_t *data);

/* --- Main Thread Function Implementation --- */

/**
 * @brief Main function for the Storage Manager thread.
 * Reads sensor data (prioritizing a local retry queue) and inserts it into the SQLite database.
 * Handles database connection errors with retries and uses the local queue to avoid data loss on temporary failures.
 * Checks the global terminate_flag for graceful shutdown requests.
 *
 * @param arg Pointer to storagemgt_args_t containing thread arguments (e.g., pointer to sbuffer).
 * @return Always returns NULL. The thread exits internally on critical errors or termination signals.
 */
void *storagemgt_run(void *arg) {
    sbuffer_t *buffer = ((storagemgt_args_t *)arg)->buffer; /* Get buffer from args */
    sqlite3 *db = NULL;             /* Database connection handle */
    gateway_error_t db_ret;         /* Return value from DB operations */
    int retry_count = 0;            /* Counter for DB connection retries */
    bool db_connected = false;      /* Flag indicating current DB connection status */
    sensor_data_t current_data;     /* Holds data currently being processed */
    bool processing_retry_item = false; /* True if current_data is from the retry queue */

    log_message(LOG_LEVEL_INFO, "Storage manager thread started."); 

    /* Initialize local retry queue */
    if (init_retry_queue(RETRY_QUEUE_INITIAL_CAPACITY) != GATEWAY_SUCCESS) {
        log_message(LOG_LEVEL_FATAL, "Storage manager failed to initialize retry queue. Exiting."); 
        return NULL;
    }

    /* 1. Initial Database Connection Attempt */
    while (retry_count < DB_CONNECT_RETRY_ATTEMPTS && !db_connected) {
        if (terminate_flag) {
            log_message(LOG_LEVEL_INFO, "Storage manager terminated during initial DB connect."); 
            goto cleanup_exit_storagemgt; /* Go directly to cleanup */
        }

        db_ret = db_connect(DB_NAME, &db); // db_connect logs internally
        if (db_ret == GATEWAY_SUCCESS) {
            db_connected = true;
        } else {
            retry_count++;
             
            log_message(LOG_LEVEL_WARNING,
                         "Failed to connect to SQL server (Attempt %d/%d). Retrying in %d seconds...",
                         retry_count, DB_CONNECT_RETRY_ATTEMPTS, DB_CONNECT_RETRY_DELAY_SEC);
            if (retry_count < DB_CONNECT_RETRY_ATTEMPTS) {
                interruptible_sleep(DB_CONNECT_RETRY_DELAY_SEC); /* Use interruptible sleep */
            }
        }
    }

    if (!db_connected) {
        
        log_message(LOG_LEVEL_FATAL,
                    "Unable to connect to SQL server %s after %d attempts. Signaling main process to exit.",
                    DB_NAME, DB_CONNECT_RETRY_ATTEMPTS);

        /* === Inform to Parent process === */
        pid_t parent_pid = getppid();
        if (parent_pid > 1) {
            log_message(LOG_LEVEL_INFO, "Sending SIGTERM to main process."); 
            if (kill(parent_pid, SIGTERM ) == -1) {
                log_message(LOG_LEVEL_ERROR, "Failed to send SIGTERM to parent: %s", strerror(errno));
            }
        } else {
            log_message(LOG_LEVEL_WARNING, "Could not get valid parent PID to signal."); 
        }
        /* ==================================== */

        goto cleanup_exit_storagemgt; // Jump to cleanup before returning NULL
    }

    /* 2. Main Loop */
    while (true) {
        gateway_error_t sbuf_ret;
        gateway_error_t insert_ret;

        /* Handle DB connection loss: Try to reconnect */
        if (!db_connected) {
            log_message(LOG_LEVEL_INFO, "Database connection lost previously. Attempting to reconnect..."); 
            retry_count = 0;
            while (retry_count < DB_CONNECT_RETRY_ATTEMPTS && !db_connected) {
                if (terminate_flag) {
                    log_message(LOG_LEVEL_INFO, "Storage manager terminated during DB reconnect attempt."); 
                    goto cleanup_exit_storagemgt;
                }
                /* Ensure previous handle is closed if attempting reconnect */
                if (db != NULL) {
                    db_disconnect(db); db = NULL; // db_disconnect logs internally
                }

                db_ret = db_connect(DB_NAME, &db); // db_connect logs internally
                if (db_ret == GATEWAY_SUCCESS) {
                    db_connected = true;
                } else {
                    retry_count++;                   
                    log_message(LOG_LEVEL_WARNING,
                                 "Failed to reconnect to SQL server (Attempt %d/%d). Retrying in %d seconds...",
                                 retry_count, DB_CONNECT_RETRY_ATTEMPTS, DB_CONNECT_RETRY_DELAY_SEC);
                    if (retry_count < DB_CONNECT_RETRY_ATTEMPTS) {
                        interruptible_sleep(DB_CONNECT_RETRY_DELAY_SEC);
                    }
                }
            }
            if (!db_connected) {       
                log_message(LOG_LEVEL_FATAL,
                             "Failed to re-establish connection to SQL server %s after %d attempts. Signaling main process to exit.",
                             DB_NAME, DB_CONNECT_RETRY_ATTEMPTS);

                /* === Inform to Parent === */
                pid_t parent_pid = getppid();
                if (parent_pid > 1) {
                     log_message(LOG_LEVEL_INFO, "Sending SIGTERM to main process."); 
                    if (kill(parent_pid, SIGTERM ) == -1) {
                        log_message(LOG_LEVEL_ERROR, "Failed to send SIGTERM to parent: %s", strerror(errno)); 
                    }
                } else {
                    log_message(LOG_LEVEL_WARNING, "Could not get valid parent PID to signal."); 
                }
                /* ==================================== */

                goto cleanup_exit_storagemgt; // Jump to cleanup before returning NULL
           }
        } /* End reconnection logic */

        /* Now DB should be connected */
        processing_retry_item = false; /* Reset flag for this iteration */

        /* Prioritize processing items from the local retry queue */
        if (!is_retry_queue_empty()) {
            if (peek_retry_item(&current_data) == GATEWAY_SUCCESS) {
                processing_retry_item = true; /* Mark data as coming from retry queue */ 
                log_message(LOG_LEVEL_DEBUG, "Attempting to insert item from retry queue (Sensor %d)", current_data.id);
            } else {
                log_message(LOG_LEVEL_ERROR, "Could not peek item from non-empty retry queue! Skipping retry this iteration."); 
                 /* Continue to try reading from sbuffer */
            }
        }

        /* If retry queue was empty or peek failed, read from the main shared buffer */
        if (!processing_retry_item) {
            sbuf_ret = sbuffer_remove(buffer, &current_data); /* Read from sbuffer */

            if (sbuf_ret == SBUFFER_SHUTDOWN) {
                log_message(LOG_LEVEL_INFO, "Storage manager received shutdown signal from sbuffer. Exiting loop."); 
                break;
            }
            else if (sbuf_ret != GATEWAY_SUCCESS) {
                if (sbuf_ret == SBUFFER_EMPTY) { /* Assuming buffer signals empty on free/shutdown */
                    log_message(LOG_LEVEL_INFO, "Storage manager sbuffer remove returned empty/error, likely shutting down."); 
                    break; /* Exit loop */
                } else {
                    log_message(LOG_LEVEL_ERROR, "Storage manager failed to remove data from sbuffer (Error %d)", sbuf_ret); 
                    interruptible_sleep(1); /* Short wait before trying again */
                    continue; /* Try reading sbuffer again */
                }
            }
             
            log_message(LOG_LEVEL_DEBUG, "Read new item from sbuffer (Sensor %d)", current_data.id);
        }

        /* Attempt to insert the current data item (from retry queue or sbuffer) */
        insert_ret = db_insert_sensor_data(db, &current_data); // db_insert logs internally on success/failure

        if (insert_ret == GATEWAY_SUCCESS) {
            /* If the successfully inserted item was from the retry queue, remove it now */
            if (processing_retry_item) {
                sensor_data_t removed_item; // Need a variable to pass to dequeue
                if (dequeue_retry_item(&removed_item) != GATEWAY_SUCCESS) {
                    /* This indicates an inconsistency */
                    log_message(LOG_LEVEL_ERROR, "Failed to dequeue item from retry queue after successful insert!"); 
                } else {
                     
                    log_message(LOG_LEVEL_DEBUG, "Dequeued Sensor ID: %d from retry queue.", removed_item.id);
                }
            }
            /* If item was from sbuffer, we are done with it */

        } else { /* Insert failed */
            // db_insert_sensor_data already logged the failure details at ERROR level

            /* Assume connection is lost on any insert error */
            log_message(LOG_LEVEL_WARNING, "Assuming database connection lost due to insert error."); 
            db_connected = false;

            /* If the failed item was NEW data from sbuffer, add it to retry queue */
            if (!processing_retry_item) {
                if (enqueue_retry_item(&current_data) != GATEWAY_SUCCESS) {
                    // enqueue logs internally on failure
                } else {
                    // enqueue logs internally on success
                }
            } else {
                /* Item was already from retry queue and failed again */
                /* It remains at the head of the queue */                 
                log_message(LOG_LEVEL_WARNING, "Retry insert failed for Sensor ID: %d. Item remains in queue.", current_data.id);
                /* Consider adding a max retry count per item here */
            }
            /* Loop will continue and attempt to reconnect */
        }

    } /* End of main while loop */

cleanup_exit_storagemgt: /* Label for cleanup and exit */
    /* 3. Cleanup */
    log_message(LOG_LEVEL_INFO, "Storage manager thread shutting down..."); 
    if (db != NULL) {
        db_disconnect(db); // db_disconnect logs internally
        db = NULL;
    }
    free_retry_queue(); /* Free the local retry queue */
    log_message(LOG_LEVEL_INFO, "Storage manager finished cleanup."); 

    return NULL;
}

/* --- Shutdown Function Implementation --- */

/**
 * @brief Signals the Storage Manager thread to stop gracefully.
 * (Currently relies on terminate_flag being checked in the run loop).
 */
void storagemgt_stop(void) {
    log_message(LOG_LEVEL_INFO, "Storage Manager stop requested."); 
    // Actual stop happens when terminate_flag is checked in the run loop
}

/* --- Implementation of Internal Helper Functions --- */

/**
 * @brief Sleeps for a specified duration, checking the termination flag periodically.
 * Uses nanosleep for better interruptibility.
 * 
 * @param seconds Total seconds to sleep.
 */
static void interruptible_sleep(unsigned int seconds) {
    struct timespec sleep_ts; /* Time to sleep in each nanosleep call */
    struct timespec rem_ts;   /* Remaining time if nanosleep is interrupted */
    time_t end_time = time(NULL) + seconds; /* When the total sleep should end */

    /* Set the sleep interval (e.g., 100ms) */
    sleep_ts.tv_sec = 0;
    sleep_ts.tv_nsec = (long)SHORT_SLEEP_MS * 1000000L;
    /* Handle potential overflow if SHORT_SLEEP_MS is large */
    if (sleep_ts.tv_nsec >= 1000000000L) {
        sleep_ts.tv_sec = sleep_ts.tv_nsec / 1000000000L;
        sleep_ts.tv_nsec %= 1000000000L;
    }

    /* Loop until the target end time is reached or termination is flagged */
    while (time(NULL) < end_time && !terminate_flag) {
        int ret = nanosleep(&sleep_ts, &rem_ts);

        if (ret == -1 && errno == EINTR) {
            /* Interrupted by signal, check flag and continue loop */
            log_message(LOG_LEVEL_DEBUG, "nanosleep interrupted by signal."); 
            if (terminate_flag) break; /* Exit sleep early */
            /* If not terminating, loop continues and time check handles duration */
        } else if (ret == -1) {
            /* Other nanosleep error */
            log_message(LOG_LEVEL_WARNING, "nanosleep error during interruptible sleep: %s", strerror(errno)); 
            break; /* Exit sleep loop on other errors */
        }
        /* Nanosleep completed the interval successfully */
    }
    if(terminate_flag) log_message(LOG_LEVEL_DEBUG, "Sleep interrupted by termination flag."); 
}

/* --- Local Retry Queue Implementation (Simple Circular Array) --- */

/**
 * @brief Initializes the local retry queue.
 * 
 * @param initial_capacity The initial size of the queue.
 * @return GATEWAY_SUCCESS or GATEWAY_ERROR_NOMEM.
 */
static gateway_error_t init_retry_queue(int initial_capacity) {
    if (initial_capacity <= 0) initial_capacity = RETRY_QUEUE_INITIAL_CAPACITY;
    retry_queue.items = malloc(initial_capacity * sizeof(sensor_data_t));
    if (retry_queue.items == NULL) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for retry queue: %s", strerror(errno)); 
        return GATEWAY_ERROR_NOMEM;
    }
    retry_queue.capacity = initial_capacity;
    retry_queue.count = 0;
    retry_queue.head = 0;
    retry_queue.tail = 0;
    queue_initialized = true;
    log_message(LOG_LEVEL_INFO, "Local retry queue initialized with capacity %d.", initial_capacity); 
    return GATEWAY_SUCCESS;
}

/**
 * @brief Frees the memory used by the retry queue.
 */
static void free_retry_queue(void) {
    if (queue_initialized && retry_queue.items != NULL) {
        free(retry_queue.items);
        retry_queue.items = NULL;
        queue_initialized = false;
        log_message(LOG_LEVEL_INFO, "Local retry queue freed."); 
    }
}

/**
 * @brief Checks if the retry queue is empty.
 * 
 * @return True if the queue is empty, false otherwise.
 */
static bool is_retry_queue_empty(void) {
    return (!queue_initialized || retry_queue.count == 0);
}

/**
 * @brief Checks if the retry queue is full.
 * 
 * @return True if the queue is full, false otherwise.
 */
static bool is_retry_queue_full(void) {
    return (queue_initialized && retry_queue.count >= retry_queue.capacity);
}

/**
 * @brief Adds an item to the tail of the retry queue.
 * If the queue is full, it currently drops the oldest item.
 * 
 * @param data Pointer to the sensor data item to add.
 * @return GATEWAY_SUCCESS, or GATEWAY_ERROR if not initialized or enqueue fails.
 */
static gateway_error_t enqueue_retry_item(const sensor_data_t *data) {
    if (!queue_initialized) return GATEWAY_ERROR;
    if (data == NULL) return GATEWAY_ERROR_INVALID_ARG;

    if (is_retry_queue_full()) {
        /* Strategy when full: Drop the oldest item */
        log_message(LOG_LEVEL_WARNING, "Retry queue full (capacity %d). Dropping oldest item to make space.", retry_queue.capacity); 
        sensor_data_t dummy;
        if (dequeue_retry_item(&dummy) != GATEWAY_SUCCESS) {
            log_message(LOG_LEVEL_ERROR, "Failed to drop oldest item from full retry queue."); 
            return GATEWAY_ERROR; /* Could not make space */
        } else {
            log_message(LOG_LEVEL_WARNING, "Dropped item (Sensor ID: %d, TS: %ld) from retry queue.", dummy.id, dummy.ts); 
        }
    }

    /* Add item to tail */
    retry_queue.items[retry_queue.tail] = *data; /* Copy data */
    retry_queue.tail = (retry_queue.tail + 1) % retry_queue.capacity;
    retry_queue.count++;
    log_message(LOG_LEVEL_DEBUG, "Enqueued Sensor ID: %d to retry queue (count: %d)", data->id, retry_queue.count); 
    return GATEWAY_SUCCESS;
}

/**
 * @brief Removes the oldest item (at head) from the retry queue.
 * 
 * @param data Pointer to store the removed item's data.
 * @return GATEWAY_SUCCESS, or GATEWAY_ERROR_INVALID_ARG if queue is empty or not initialized.
 */
static gateway_error_t dequeue_retry_item(sensor_data_t *data) {
    if (!queue_initialized || is_retry_queue_empty()) {
        return GATEWAY_ERROR_INVALID_ARG; // Return error, don't log here, caller should log if needed
    }
    if (data == NULL) return GATEWAY_ERROR_INVALID_ARG;

    /* Copy data out from head */
    *data = retry_queue.items[retry_queue.head];
    /* Move head */
    retry_queue.head = (retry_queue.head + 1) % retry_queue.capacity;
    retry_queue.count--;
    log_message(LOG_LEVEL_DEBUG, "Dequeued Sensor ID: %d from retry queue (count: %d)", data->id, retry_queue.count);
    return GATEWAY_SUCCESS;
}

/**
 * @brief Gets a copy of the oldest item (at head) without removing it.
 * 
 * @param data Pointer to store the peeked item's data.
 * @return GATEWAY_SUCCESS, or GATEWAY_ERROR_INVALID_ARG if queue is empty or not initialized.
 */
static gateway_error_t peek_retry_item(sensor_data_t *data) {
    if (!queue_initialized || is_retry_queue_empty()) {
        return GATEWAY_ERROR_INVALID_ARG; // Return error, don't log here
    }
    if (data == NULL) return GATEWAY_ERROR_INVALID_ARG;

    /* Copy data out from head */
    *data = retry_queue.items[retry_queue.head];
    log_message(LOG_LEVEL_DEBUG, "Peeked Sensor ID: %d from retry queue", data->id);
    return GATEWAY_SUCCESS;
}