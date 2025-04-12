#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h> /* Required for bool type */
#include "common.h"  /* Required for gateway_error_t */
#include <stdarg.h> /* Required for va_list */

/* Define Log Levels */
typedef enum {
    LOG_LEVEL_FATAL = 0, // Errors causing termination
    LOG_LEVEL_ERROR,     // Runtime errors that don't stop execution
    LOG_LEVEL_WARNING,   // Potential issues
    LOG_LEVEL_INFO,      // Informational messages
    LOG_LEVEL_DEBUG      // Detailed debug information
} log_level_t;

/**
 * Initializes the logging system.
 * This might involve creating/opening the FIFO.
 * Should be called appropriately by the main process and potentially the log process.
 * @return GATEWAY_SUCCESS on success, an error code otherwise (e.g., LOGGER_FIFO_CREATE_ERR).
 */
gateway_error_t logger_init(void);

/**
 * Opens the FIFO for writing. Called by the main process AFTER fork().
 * @return GATEWAY_SUCCESS on success, LOGGER_FIFO_OPEN_ERR otherwise.
 */
gateway_error_t logger_open_write_fifo(void);

/**
 * @brief Logs a formatted message with a specific log level to the FIFO.
 * This function is thread-safe.
 *
 * @param level The log level (e.g., LOG_LEVEL_INFO).
 * @param format The format string (like printf).
 * @param ... Variable arguments corresponding to the format string.
 */
 void log_message(log_level_t level, const char *format, ...);

/**
 * Cleans up logging resources.
 * This might involve closing any open file descriptors (like the FIFO write end).
 */
void logger_cleanup(void);

/**
 * The main function for the dedicated log process.
 * Reads log messages from the FIFO and writes them to the log file.
 * This function runs indefinitely until an error occurs or termination is signaled.
 */
void run_log_process(void);

#endif /* LOGGER_H */