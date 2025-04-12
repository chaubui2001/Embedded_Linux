#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>      /* For open() and O_WRONLY */
#include <unistd.h>     /* For write(), close(), unlink() */
#include <sys/stat.h>   /* For mkfifo() and mode constants */
#include <sys/types.h>  /* For mode_t */
#include <errno.h>      /* For errno */
#include <pthread.h>    /* For mutex */
#include <limits.h>     /* For PIPE_BUF */
#include <stdbool.h>    /* For bool */

/* Include project-specific headers */
#include "config.h"     /* For LOG_FIFO_NAME */
#include "common.h"     /* For gateway_error_t */
#include "logger.h"     /* For function declarations */

/* Static variables for the logger module */
static int fifo_fd = -1;            /* File descriptor for writing to the FIFO */
static pthread_mutex_t log_mutex;   /* Mutex to ensure thread-safe writes to FIFO */
static bool mutex_initialized = false; /* Track mutex init status */
static bool fifo_created = false;     /* Track FIFO creation attempt */

/* Define permissions for the FIFO (owner read/write, group read/write) */
#define FIFO_PERMISSIONS 0660

/* Map log levels to strings */
static const char* log_level_strings[] = {
    "[FATAL]  ",
    "[ERROR]  ",
    "[WARNING]",
    "[INFO]   ",
    "[DEBUG]  "
};

/**
 * @brief Initializes the mutex and creates the FIFO (if necessary).
 * Does NOT open the FIFO for writing.
 * @return GATEWAY_SUCCESS on success, an error code otherwise.
 */
 gateway_error_t logger_init(void) {

    /* 1. Initialize the mutex */
    if (pthread_mutex_init(&log_mutex, NULL) != 0) {
        perror("Logger ERROR: Failed to initialize mutex");
        return THREAD_MUTEX_INIT_ERR;
    }
    mutex_initialized = true;

    /* 2. Create the FIFO */
    if (mkfifo(LOG_FIFO_NAME, FIFO_PERMISSIONS) == -1) {
        if (errno != EEXIST) {
            perror("Logger ERROR: Failed to create FIFO");
            /* Don't destroy mutex here, main might still need cleanup */
            return LOGGER_FIFO_CREATE_ERR;
        }
        fprintf(stderr, "Logger INFO: FIFO '%s' already exists.\n", LOG_FIFO_NAME);
    } else {
        fprintf(stderr, "Logger INFO: FIFO '%s' created successfully.\n", LOG_FIFO_NAME);
    }
    fifo_created = true; /* Mark that creation was attempted/successful */

    return GATEWAY_SUCCESS;
}

/**
 * @brief Opens the FIFO for writing. Called by the main process AFTER fork().
 * @return GATEWAY_SUCCESS on success, LOGGER_FIFO_OPEN_ERR otherwise.
 */
gateway_error_t logger_open_write_fifo(void) {
    if (fifo_fd >= 0) {
        return GATEWAY_SUCCESS; // Already open
    }
    if (!fifo_created) {
        fprintf(stderr, "Logger ERROR: Cannot open FIFO write end before FIFO is created (call logger_init first).\n");
        return LOGGER_ERROR; // Or a specific error
    }

    fprintf(stderr, "Logger INFO: Opening FIFO '%s' for writing...\n", LOG_FIFO_NAME);
    /* Open the FIFO for writing, blocking if no reader is present */
    fifo_fd = open(LOG_FIFO_NAME, O_WRONLY);
    if (fifo_fd == -1) {
        perror("Logger ERROR: Failed to open FIFO for writing");
        return LOGGER_FIFO_OPEN_ERR;
    }
    fprintf(stderr, "Logger INFO: FIFO '%s' opened successfully for writing.\n", LOG_FIFO_NAME);
    return GATEWAY_SUCCESS;
}

/**
 * @brief Logs a formatted message with a specific log level to the FIFO. (Thread-safe).
 */
 void log_message(log_level_t level, const char *format, ...) {
    /* Basic checks */
    if (fifo_fd < 0 || format == NULL) {
        fprintf(stderr, "Logger ERROR: FIFO not open or invalid format. Log attempt ignored.\n");
        return;
    }
    if (!mutex_initialized) { 
        fprintf(stderr, "Logger ERROR: Mutex not initialized. Log attempt ignored.\n");
        return;
    }
    if (level < LOG_LEVEL_FATAL || level > LOG_LEVEL_DEBUG) {
        level = LOG_LEVEL_INFO; // Default to INFO if level is invalid
        fprintf(stderr, "Logger WARN: Invalid log level provided, defaulting to INFO.\n");
    }


    char user_message[PIPE_BUF / 2]; // Buffer for the user's formatted message
    char final_buffer[PIPE_BUF];   // Final buffer including timestamp, level, and message
    va_list args;

    /* Format the user message */
    va_start(args, format);
    /* Use vsnprintf for safe formatting into user_message buffer */
    int user_msg_len = vsnprintf(user_message, sizeof(user_message), format, args);
    va_end(args);

    if (user_msg_len < 0) {
        fprintf(stderr, "Logger ERROR: vsnprintf formatting failed. Log attempt ignored.\n");
        return; // Formatting error
    }


    /* Get current time for timestamp */
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    char time_str[30]; // Buffer for "YYYY-MM-DD HH:MM:SS"
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local_time);


    /* Construct the final log entry: Timestamp Level Message */
    /* Ensure enough space for timestamp, level prefix, message, space, newline, and null terminator */
    int final_len = snprintf(final_buffer, sizeof(final_buffer), "%s %s%s\n",
                             time_str,
                             log_level_strings[level],
                             user_message);

    if (final_len < 0) {
        fprintf(stderr, "Logger ERROR: snprintf failed constructing final log entry. Log attempt ignored.\n");
        return; /* Formatting error */
    }
     if ((size_t)final_len >= sizeof(final_buffer)) {
        fprintf(stderr, "Logger WARN: Final log message truncated before writing to FIFO.\n");
        /* Ensure null termination if truncated exactly at the end */
        final_buffer[sizeof(final_buffer) - 1] = '\n'; // Make sure it ends with newline
        final_buffer[sizeof(final_buffer) - 2] = '.'; // Indicate truncation visually
        final_buffer[sizeof(final_buffer) - 3] = '.';
        final_buffer[sizeof(final_buffer) - 4] = '.';
        final_len = sizeof(final_buffer) -1; // Use the maximum possible length (excluding null term)
        // Note: PIPE_BUF guarantees atomicity up to PIPE_BUF bytes, but the message itself might be truncated here.
    }

    /* Lock mutex */
    if (pthread_mutex_lock(&log_mutex) != 0) {
        perror("Logger CRITICAL: Failed to lock log mutex");
        fprintf(stderr, "Logger CRITICAL: Dropped log message due to mutex lock failure: %s", final_buffer);
        return;
    }

    /* Write to FIFO */
    ssize_t bytes_written = write(fifo_fd, final_buffer, final_len); // Use final_buffer and final_len modified write call

    /* Unlock mutex */
    if (pthread_mutex_unlock(&log_mutex) != 0) { //
        perror("Logger CRITICAL: Failed to unlock log mutex");
    }

    /* Check write result */
    if (bytes_written == -1) { 
        if (errno == EPIPE) { 
            fprintf(stderr, "Logger ERROR: FIFO write failed (Broken pipe - log process likely dead).\n");
            close(fifo_fd); 
            fifo_fd = -1; 
        } else {
            perror("Logger ERROR: Failed to write to FIFO"); 
        }
    } else if (bytes_written < final_len) { // Modified comparison
        fprintf(stderr, "Logger WARN: Partial write to FIFO (%zd/%d bytes).\n", bytes_written, final_len);
    }
}

/**
 * @brief Cleans up logging resources used by the main process.
 */
void logger_cleanup(void) {
    fprintf(stderr, "Logger INFO: Cleaning up logger resources...\n");

    /* Close the FIFO write end if open */
    if (fifo_fd >= 0) {
        if (close(fifo_fd) == -1) {
            perror("Logger WARN: Failed to close FIFO write end");
        }
        fifo_fd = -1; /* Mark as closed */
    }
    else{
        fprintf(stderr, "Logger INFO: FIFO write end already closed or not opened.\n");
    }

    /* Destroy the mutex if initialized */
    if (mutex_initialized) {
        if (pthread_mutex_destroy(&log_mutex) != 0) {
            perror("Logger WARN: Failed to destroy log mutex");
        }
        mutex_initialized = false;
    }

    /* Remove the FIFO file from the filesystem */
    /* Only attempt unlink if we know creation was attempted/successful */
    if (fifo_created) {
        if (unlink(LOG_FIFO_NAME) == -1) {
            if (errno != ENOENT) { /* ENOENT (No such file or directory) is okay */
                perror("Logger WARN: Failed to remove FIFO file");
            }
        } else {
            fprintf(stderr, "Logger INFO: FIFO '%s' removed.\n", LOG_FIFO_NAME);
        }
        fifo_created = false; /* Reset flag */
    }

    fprintf(stderr, "Logger INFO: Logger cleanup complete.\n");
}

/* Implementation for run_log_process() resides in log_process.c */