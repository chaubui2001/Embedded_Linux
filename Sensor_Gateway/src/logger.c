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
static bool mutex_initialized = false; /* Tracks whether the mutex has been initialized */
static bool fifo_created = false;     /* Tracks whether the FIFO has been created */

/* Define permissions for the FIFO (owner read/write, group read/write) */
#define FIFO_PERMISSIONS 0660

/* Map log levels to their corresponding string representations */
static const char* log_level_strings[] = {
    "[FATAL]  ",
    "[ERROR]  ",
    "[WARNING]",
    "[INFO]   ",
    "[DEBUG]  "
};

/**
 * @brief Initializes the logger module.
 * 
 * This function initializes the mutex and creates the FIFO if it does not already exist.
 * It does NOT open the FIFO for writing.
 * 
 * @return GATEWAY_SUCCESS on success, or an appropriate error code on failure.
 */
gateway_error_t logger_init(void) {
    /* Initialize the mutex */
    if (pthread_mutex_init(&log_mutex, NULL) != 0) {
        perror("Logger ERROR: Failed to initialize mutex");
        return THREAD_MUTEX_INIT_ERR;
    }
    mutex_initialized = true;

    /* Create the FIFO */
    if (mkfifo(LOG_FIFO_NAME, FIFO_PERMISSIONS) == -1) {
        if (errno != EEXIST) {
            perror("Logger ERROR: Failed to create FIFO");
            return LOGGER_FIFO_CREATE_ERR;
        }
        fprintf(stderr, "Logger INFO: FIFO '%s' already exists.\n", LOG_FIFO_NAME);
    } else {
        fprintf(stderr, "Logger INFO: FIFO '%s' created successfully.\n", LOG_FIFO_NAME);
    }
    fifo_created = true;

    return GATEWAY_SUCCESS;
}

/**
 * @brief Opens the FIFO for writing.
 * 
 * This function should be called by the main process after a fork operation.
 * 
 * @return GATEWAY_SUCCESS on success, or LOGGER_FIFO_OPEN_ERR on failure.
 */
gateway_error_t logger_open_write_fifo(void) {
    /* Check if the FIFO is already open */
    if (fifo_fd >= 0) {
        return GATEWAY_SUCCESS;
    }

    /* Ensure the FIFO has been created */
    if (!fifo_created) {
        fprintf(stderr, "Logger ERROR: Cannot open FIFO write end before FIFO is created (call logger_init first).\n");
        return LOGGER_ERROR;
    }

    fprintf(stderr, "Logger INFO: Opening FIFO '%s' for writing...\n", LOG_FIFO_NAME);

    /* Open the FIFO for writing */
    fifo_fd = open(LOG_FIFO_NAME, O_WRONLY);
    if (fifo_fd == -1) {
        perror("Logger ERROR: Failed to open FIFO for writing");
        return LOGGER_FIFO_OPEN_ERR;
    }

    fprintf(stderr, "Logger INFO: FIFO '%s' opened successfully for writing.\n", LOG_FIFO_NAME);
    return GATEWAY_SUCCESS;
}

/**
 * @brief Logs a formatted message with a specific log level to the FIFO.
 * 
 * This function is thread-safe and ensures that log messages are written atomically.
 * 
 * @param level The log level (e.g., LOG_LEVEL_FATAL, LOG_LEVEL_ERROR).
 * @param format The format string for the log message (similar to printf).
 */
void log_message(log_level_t level, const char *format, ...) {
    /* Validate input parameters */
    if (fifo_fd < 0 || format == NULL) {
        fprintf(stderr, "Logger ERROR: FIFO not open or invalid format. Log attempt ignored.\n");
        return;
    }
    if (!mutex_initialized) {
        fprintf(stderr, "Logger ERROR: Mutex not initialized. Log attempt ignored.\n");
        return;
    }
    if (level < LOG_LEVEL_FATAL || level > LOG_LEVEL_DEBUG) {
        level = LOG_LEVEL_INFO; /* Default to INFO if the level is invalid */
        fprintf(stderr, "Logger WARN: Invalid log level provided, defaulting to INFO.\n");
    }

    /* Buffers for the log message */
    char user_message[PIPE_BUF / 2]; /* Buffer for the user's formatted message */
    char final_buffer[PIPE_BUF];    /* Final buffer including timestamp, level, and message */
    va_list args;

    /* Format the user message */
    va_start(args, format);
    int user_msg_len = vsnprintf(user_message, sizeof(user_message), format, args);
    va_end(args);

    if (user_msg_len < 0) {
        fprintf(stderr, "Logger ERROR: vsnprintf formatting failed. Log attempt ignored.\n");
        return;
    }

    /* Get the current time for the timestamp */
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    char time_str[30]; /* Buffer for "YYYY-MM-DD HH:MM:SS" */
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local_time);

    /* Construct the final log entry */
    int final_len = snprintf(final_buffer, sizeof(final_buffer), "%s %s%s\n",
                             time_str,
                             log_level_strings[level],
                             user_message);

    if (final_len < 0) {
        fprintf(stderr, "Logger ERROR: snprintf failed constructing final log entry. Log attempt ignored.\n");
        return;
    }
    if ((size_t)final_len >= sizeof(final_buffer)) {
        fprintf(stderr, "Logger WARN: Final log message truncated before writing to FIFO.\n");
        final_buffer[sizeof(final_buffer) - 1] = '\n';
        final_buffer[sizeof(final_buffer) - 2] = '.';
        final_buffer[sizeof(final_buffer) - 3] = '.';
        final_buffer[sizeof(final_buffer) - 4] = '.';
        final_len = sizeof(final_buffer) - 1;
    }

    /* Lock the mutex */
    if (pthread_mutex_lock(&log_mutex) != 0) {
        perror("Logger CRITICAL: Failed to lock log mutex");
        fprintf(stderr, "Logger CRITICAL: Dropped log message due to mutex lock failure: %s", final_buffer);
        return;
    }

    /* Write the log message to the FIFO */
    ssize_t bytes_written = write(fifo_fd, final_buffer, final_len);

    /* Unlock the mutex */
    if (pthread_mutex_unlock(&log_mutex) != 0) {
        perror("Logger CRITICAL: Failed to unlock log mutex");
    }

    /* Check the result of the write operation */
    if (bytes_written == -1) {
        if (errno == EPIPE) {
            fprintf(stderr, "Logger ERROR: FIFO write failed (Broken pipe - log process likely dead).\n");
            close(fifo_fd);
            fifo_fd = -1;
        } else {
            perror("Logger ERROR: Failed to write to FIFO");
        }
    } else if (bytes_written < final_len) {
        fprintf(stderr, "Logger WARN: Partial write to FIFO (%zd/%d bytes).\n", bytes_written, final_len);
    }
}

/**
 * @brief Cleans up resources used by the logger module.
 * 
 * This function closes the FIFO, destroys the mutex, and removes the FIFO file from the filesystem.
 */
void logger_cleanup(void) {
    fprintf(stderr, "Logger INFO: Cleaning up logger resources...\n");

    /* Close the FIFO write end if it is open */
    if (fifo_fd >= 0) {
        if (close(fifo_fd) == -1) {
            perror("Logger WARN: Failed to close FIFO write end");
        }
        fifo_fd = -1;
    } else {
        fprintf(stderr, "Logger INFO: FIFO write end already closed or not opened.\n");
    }

    /* Destroy the mutex if it has been initialized */
    if (mutex_initialized) {
        if (pthread_mutex_destroy(&log_mutex) != 0) {
            perror("Logger WARN: Failed to destroy log mutex");
        }
        mutex_initialized = false;
    }

    /* Remove the FIFO file from the filesystem */
    if (fifo_created) {
        if (unlink(LOG_FIFO_NAME) == -1) {
            if (errno != ENOENT) {
                perror("Logger WARN: Failed to remove FIFO file");
            }
        } else {
            fprintf(stderr, "Logger INFO: FIFO '%s' removed.\n", LOG_FIFO_NAME);
        }
        fifo_created = false;
    }

    fprintf(stderr, "Logger INFO: Logger cleanup complete.\n");
}

/* Implementation for run_log_process() resides in log_process.c */