#ifndef STORAGEMGT_H
#define STORAGEMGT_H

#include "sbuffer.h" /* Required for sbuffer_t */
#include "common.h"  /* Required for gateway_error_t */

/* Structure to pass arguments to the storage manager thread */
typedef struct {
    sbuffer_t *buffer; /* Pointer to the shared buffer */
} storagemgt_args_t;

/**
 * The main function for the Storage Manager thread.
 * Reads sensor data from the shared buffer, connects to the SQLite database 
 * (with retry logic), and inserts the data.
 * @param arg A pointer to a storagemgr_args_t struct containing thread arguments.
 * @return Always returns NULL. Errors should be handled internally or logged. 
 * The thread should terminate if DB connection fails permanently after retries.
 */
void *storagemgt_run(void *arg);

/* --- Shutdown Function --- */
/**
 * @brief Signals the Storage Manager thread to stop gracefully.
 */
void storagemgt_stop(void);

#endif /* STORAGEMGT_H */