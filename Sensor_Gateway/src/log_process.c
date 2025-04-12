#include <stdio.h>      /* Standard I/O functions like snprintf, fopen, fprintf, fclose */
#include <stdlib.h>     /* For exit, EXIT_FAILURE, EXIT_SUCCESS, malloc, free */
#include <string.h>     /* String manipulation functions like strerror, strlen, memset, etc. */
#include <fcntl.h>      /* File control options (e.g., O_RDONLY) */
#include <unistd.h>     /* POSIX functions like read, write, close */
#include <sys/stat.h>   /* For FIFO-related constants if needed */
#include <time.h>       /* Time-related functions like time(), localtime(), strftime() */
#include <errno.h>      /* For errno to handle error codes */
#include <stdbool.h>    /* For boolean type (true/false) */
#include <signal.h>     /* For signal handling (e.g., sig_atomic_t) */

/* Include project-specific headers */
#include "config.h"     /* Contains definitions like LOG_FIFO_NAME, LOG_FILE_NAME */
#include "common.h"     /* Contains common definitions like gateway_error_t */
#include "logger.h"     /* Contains the declaration for run_log_process */

/* --- Local Macros --- */
#define FIFO_READ_BUFFER_SIZE 512 /* Size of the buffer for each read() call */
#define ASSEMBLY_BUFFER_SIZE (FIFO_READ_BUFFER_SIZE * 4) /* Buffer to assemble potentially partial lines */
#define TIMESTAMP_BUFFER_SIZE 100 /* Buffer size for formatted timestamp */
#define LOG_LINE_BUFFER_SIZE (ASSEMBLY_BUFFER_SIZE + TIMESTAMP_BUFFER_SIZE + 50) /* Max size for final log line */
#define TIMESTAMP_FORMAT "%Y-%m-%d %H:%M:%S" /* Format for timestamps */
#define INITIAL_SEQUENCE_NUMBER 1 /* Initial sequence number for log entries */

/* --- Main Function for Log Process --- */

/**
 * @brief Main function for the dedicated log process.
 * 
 * This function reads log messages from a FIFO, handles partial reads by assembling lines,
 * and writes them to a log file with a sequence number and timestamp prepended.
 * It runs indefinitely until the FIFO's write end is closed or an unrecoverable error occurs.
 */
void run_log_process(void) {
    int fifo_fd = -1; /* File descriptor for the FIFO */
    FILE *log_file = NULL; /* File pointer for the log file */
    char read_buffer[FIFO_READ_BUFFER_SIZE]; /* Temporary buffer for read() */
    char *assembly_buffer = NULL; /* Dynamically allocated buffer to assemble lines */
    int assembly_buffer_len = 0; /* Current length of data in the assembly buffer */
    char timestamp_buffer[TIMESTAMP_BUFFER_SIZE]; /* Buffer for formatted timestamps */
    char log_line_buffer[LOG_LINE_BUFFER_SIZE]; /* Buffer for the final log line */
    ssize_t bytes_read; /* Number of bytes read from the FIFO */
    uint64_t sequence_number = INITIAL_SEQUENCE_NUMBER; /* Sequence number for log entries */
    time_t current_time; /* Current time */
    struct tm *local_time; /* Local time structure */
    bool fifo_closed = false; /* Flag to indicate if the FIFO's write end is closed */

    /* Allocate memory for the assembly buffer */
    assembly_buffer = malloc(ASSEMBLY_BUFFER_SIZE);
    if (assembly_buffer == NULL) {
        perror("Log Process CRITICAL: Failed to allocate assembly buffer");
        exit(EXIT_FAILURE);
    }
    assembly_buffer[0] = '\0'; /* Ensure the buffer is initially empty */

    /* 1. Open the FIFO for reading */
    fifo_fd = open(LOG_FIFO_NAME, O_RDONLY);
    if (fifo_fd == -1) {
        perror("Log Process CRITICAL: Failed to open FIFO for reading");
        free(assembly_buffer);
        exit(EXIT_FAILURE);
    }

    /* 2. Open the log file for appending */
    log_file = fopen(LOG_FILE_NAME, "a");
    if (log_file == NULL) {
        perror("Log Process CRITICAL: Failed to open log file for appending");
        close(fifo_fd);
        free(assembly_buffer);
        exit(EXIT_FAILURE);
    }

    /* Log a startup message */
    time(&current_time);
    local_time = localtime(&current_time);
    strftime(timestamp_buffer, sizeof(timestamp_buffer), TIMESTAMP_FORMAT, local_time);
    fprintf(log_file, "0 %s Log process started.\n", timestamp_buffer);
    fflush(log_file);

    fprintf(stderr, "Log process started. Reading from %s, writing to %s\n", LOG_FIFO_NAME, LOG_FILE_NAME); /* Info output */

    /* 3. Main loop: Read, assemble, and process lines */
    while (!fifo_closed) {
        /* Read data from the FIFO */
        bytes_read = read(fifo_fd, read_buffer, sizeof(read_buffer));

        if (bytes_read > 0) {
            /* Append read data to the assembly buffer, checking for overflow */
            if (assembly_buffer_len + bytes_read >= ASSEMBLY_BUFFER_SIZE) {
                /* Handle overflow: log error and reset the buffer */
                fprintf(stderr, "Log Process ERROR: Assembly buffer overflow. Log messages might be lost/corrupted.\n");
                fprintf(log_file, "0 %s Log Process ERROR: Assembly buffer overflow.\n", timestamp_buffer); /* Use last known timestamp */
                fflush(log_file);
                assembly_buffer_len = 0; /* Reset buffer */
                continue; /* Skip processing this chunk */
            }
            memcpy(assembly_buffer + assembly_buffer_len, read_buffer, bytes_read);
            assembly_buffer_len += bytes_read;

        } else if (bytes_read == 0) {
            /* End of File - FIFO write end closed */
            fprintf(stderr, "Log Process: FIFO write end closed.\n"); /* Info output */
            fifo_closed = true; /* Set flag to process remaining buffer and exit */

        } else { /* bytes_read < 0 */
            if (errno == EINTR) {
                continue; /* Interrupted system call, retry read */
            } else {
                perror("Log Process ERROR: Failed to read from FIFO");
                /* Log error to file before exiting */
                time(&current_time);
                local_time = localtime(&current_time);
                strftime(timestamp_buffer, sizeof(timestamp_buffer), TIMESTAMP_FORMAT, local_time);
                fprintf(log_file, "%lu %s Log process exiting due to FIFO read error: %s.\n", sequence_number, timestamp_buffer, strerror(errno));
                fflush(log_file);
                fifo_closed = true; /* Treat as EOF for cleanup */
                break; /* Exit loop on error */
            }
        }

        /* Process complete lines (ending with '\n') from the assembly buffer */
        char *current_pos = assembly_buffer;
        char *newline_pos;
        int processed_len = 0;

        while ((newline_pos = memchr(current_pos, '\n', assembly_buffer_len - processed_len)) != NULL) {
            int line_len = newline_pos - current_pos; /* Length excluding newline */

            /* Ensure the line fits in the log_line_buffer */
            if (line_len >= LOG_LINE_BUFFER_SIZE - TIMESTAMP_BUFFER_SIZE - 50) {
                fprintf(stderr, "Log Process WARN: Assembled line too long, potential truncation.\n");
                line_len = LOG_LINE_BUFFER_SIZE - TIMESTAMP_BUFFER_SIZE - 50 - 1; /* Truncate */
            }

            /* Format and write the complete line */
            time(&current_time);
            local_time = localtime(&current_time);
            strftime(timestamp_buffer, sizeof(timestamp_buffer), TIMESTAMP_FORMAT, local_time);

            /* Copy the message part and null-terminate it */
            char message_part[line_len + 1];
            memcpy(message_part, current_pos, line_len);
            message_part[line_len] = '\0';

            snprintf(log_line_buffer, sizeof(log_line_buffer),
                    "%lu %s %s", /* Format: SeqNum Timestamp Message */
                    sequence_number,
                    timestamp_buffer,
                    message_part);

            if (fprintf(log_file, "%s\n", log_line_buffer) < 0) {
                perror("Log Process ERROR: Failed to write to log file");
                fprintf(stderr, "Log Process ERROR: Failed to write: %s\n", log_line_buffer);
            } else {
                fflush(log_file);
            }

            sequence_number++;
            processed_len += (line_len + 1); /* Update total processed length including newline */
            current_pos = newline_pos + 1; /* Move start position past the newline */
        }

        /* Remove processed data from the assembly buffer by shifting remaining data */
        if (processed_len > 0) {
            assembly_buffer_len -= processed_len;
            memmove(assembly_buffer, current_pos, assembly_buffer_len); /* Use memmove for overlapping regions */
        }
    } /* End of main while loop */

    /* Process any remaining data in the assembly buffer after EOF */
    if (assembly_buffer_len > 0) {
        fprintf(stderr, "Log Process WARN: Processing remaining partial message after FIFO closed.\n"); /* Info */
        assembly_buffer[assembly_buffer_len] = '\0'; /* Null-terminate */

        time(&current_time);
        local_time = localtime(&current_time);
        strftime(timestamp_buffer, sizeof(timestamp_buffer), TIMESTAMP_FORMAT, local_time);

        snprintf(log_line_buffer, sizeof(log_line_buffer),
                "%lu %s %s [PARTIAL/EOF]", /* Mark as partial */
                sequence_number,
                timestamp_buffer,
                assembly_buffer);

        fprintf(log_file, "%s\n", log_line_buffer);
        fflush(log_file);
        sequence_number++;
    }

    /* 4. Cleanup */
    fprintf(stderr, "Log process cleaning up...\n");
    if (fifo_fd != -1) {
        close(fifo_fd);
    }
    if (log_file != NULL) {
        fprintf(log_file, "%lu %s Log process finished.\n", sequence_number, timestamp_buffer); /* Final log message */
        fclose(log_file);
    }
    if (assembly_buffer != NULL) {
        free(assembly_buffer);
    }

    fprintf(stderr, "Log process finished.\n");
    exit(EXIT_SUCCESS); /* Normal exit */
}