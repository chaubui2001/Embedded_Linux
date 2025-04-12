#define _GNU_SOURCE         /* Needed for pthread_timedjoin_np, strsignal */
#include <stdio.h>          /* Standard I/O */
#include <stdlib.h>         /* Standard Library (exit, atoi, etc.) */
#include <unistd.h>         /* POSIX API (fork, sleep, write, etc.) */
#include <pthread.h>        /* POSIX Threads */
#include <signal.h>         /* Signal handling (sigaction, sigwaitinfo, sigprocmask etc.) */
#include <sys/wait.h>       /* For waitpid */
#include <errno.h>          /* For errno */
#include <string.h>         /* For strerror, strlen, memset */
#include <limits.h>         /* For LONG_MAX, LONG_MIN */
#include <time.h>           /* For clock_gettime */
#include <stdbool.h>        /* For bool type */

/* Include project headers */
#include "config.h"         /* Configuration definitions */
#include "common.h"         /* Common utility functions and definitions */
#include "sbuffer.h"        /* Shared buffer implementation */
#include "logger.h"         /* Logger module for logging messages */
#include "conmgt.h"         /* Connection Manager module */
#include "datamgt.h"        /* Data Manager module */
#include "storagemgt.h"     /* Storage Manager module */
#include "cmdif.h"          /* Command Interface module */

/* --- Local Macros --- */
#define MIN_PORT 1           /* Minimum valid port number */
#define MAX_PORT 65535       /* Maximum valid port number */

/* --- Global Variables --- */

/* Flag to signal termination, set by signal handler */
volatile sig_atomic_t terminate_flag = 0;

/* --- Function Prototypes --- */

/**
 * @brief Prints command line usage instructions.
 * @param prog_name Name of the program (argv[0]).
 */
static void print_usage(const char *prog_name);

/**
 * @brief Signal handler for termination signals.
 * @param sig Signal number received.
 */
static void signal_handler(int sig);

/* --- Main Function --- */

int main(int argc, char *argv[]) {
    /* --- Variable Declarations --- */

    /* Configuration & Arguments */
    int server_port;                        /* Port number from command line argument */
    const char *map_filename = MAP_FILE_NAME; /* Default filename for room-sensor map */

    /* Process & Thread Management */
    pid_t log_pid = -1;                     /* Process ID of the logger child process */
    pthread_t conmgt_thread_id = 0;         /* Thread ID for Connection Manager */
    pthread_t datamgt_thread_id = 0;        /* Thread ID for Data Manager */
    pthread_t storagemgt_thread_id = 0;     /* Thread ID for Storage Manager */
    pthread_t cmdif_thread_id = 0;          /* Thread ID for Command Interface */
    bool conmgt_created = false;            /* Flag: Connection Manager thread created */
    bool datamgt_created = false;           /* Flag: Data Manager thread created */
    bool storagemgt_created = false;        /* Flag: Storage Manager thread created */
    bool cmdif_created = false;             /* Flag: Command Interface thread created */

    /* Thread Argument Structures */
    conmgt_args_t conmgt_args;              /* Arguments for Connection Manager thread */
    datamgt_args_t datamgt_args;            /* Arguments for Data Manager thread */
    storagemgt_args_t storagemgt_args;      /* Arguments for Storage Manager thread */
    cmdif_args_t cmdif_args;                /* Arguments for Command Interface thread */

    /* Shared Resources */
    sbuffer_t *buffer = NULL;               /* Pointer to the shared sensor data buffer */
    room_sensor_map_t *room_map = NULL;     /* Pointer to the loaded room-sensor map */

    /* Control Flow & Status */
    gateway_error_t ret;                    /* Return value from gateway functions */
    void *thread_result = NULL;             /* Placeholder for pthread_join result (not currently used) */
    sigset_t wait_mask;                     /* Signal set for sigwaitinfo */
    int signum_received = 0;                /* Signal number received by sigwaitinfo */

    /* Initialization specific to cmdif_args */
    cmdif_args.socket_path = CMD_SOCKET_PATH; /* Set command socket path */

    /* --- End of Variable Declarations --- */

    /* 1. Block SIGINT and SIGTERM for the main thread */
    sigemptyset(&wait_mask);
    sigaddset(&wait_mask, SIGINT);
    sigaddset(&wait_mask, SIGTERM);
    if (pthread_sigmask(SIG_BLOCK, &wait_mask, NULL) != 0) {
        perror("CRITICAL: Failed to set signal mask");
        return EXIT_FAILURE;
    }

    /* 2. Parse Command Line Arguments */
    if (argc != 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    char *endptr;
    errno = 0;
    long port_long = strtol(argv[1], &endptr, 10);
    if ((errno == ERANGE && (port_long == LONG_MAX || port_long == LONG_MIN)) || 
        (errno != 0 && port_long == 0) || 
        endptr == argv[1] || 
        *endptr != '\0' || 
        port_long < MIN_PORT || 
        port_long > MAX_PORT) {
        fprintf(stderr, "Error: Invalid port number '%s'. Must be between %d and %d.\n", argv[1], MIN_PORT, MAX_PORT);
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    server_port = (int)port_long;
    printf("INFO: Server starting on port %d\n", server_port);

    /* 3. Initialize Logger (Create FIFO & Init Mutex) - Must be BEFORE fork */
    ret = logger_init();
    if (ret != GATEWAY_SUCCESS) {
        fprintf(stderr, "CRITICAL: Failed to initialize logger base (Error %d). Exiting.\n", ret);
        return EXIT_FAILURE;
    }

    /* 4. Load Room-Sensor Map (Optional) */
    #ifdef DATAMGT_H /* Only if datamgt module is included */
    ret = datamgt_load_room_sensor_map(map_filename, &room_map); // datamgt_load logs internally now
    if (ret != GATEWAY_SUCCESS) {
        fprintf(stderr, "WARN: Failed to load room sensor map '%s'. Continuing without map.\n", map_filename);
        room_map = NULL;
    } else {
        fprintf(stderr, "INFO: Room sensor map '%s' loaded successfully (%d entries).\n", map_filename, room_map ? room_map->count : 0); 
    }
    #else
    room_map = NULL;
    #endif

    /* 5. Fork Log Process */
    log_pid = fork();
    if (log_pid == -1) {
        perror("CRITICAL: fork() failed");
        #ifdef DATAMGT_H
        datamgt_free_room_sensor_map(&room_map);
        #endif
        logger_cleanup(); /* Clean up mutex/FIFO file if created */
        return EXIT_FAILURE;
    } else if (log_pid == 0) {
        /* --- Child Process (Log Process) --- */
        run_log_process();
        fprintf(stderr,"CRITICAL: Log process function returned unexpectedly!\n");
        exit(EXIT_FAILURE);
    }

    /* --- Parent Process (Main Gateway Process) --- */
    printf("INFO: Main process (PID: %d) started, Log process PID: %d\n", getpid(), log_pid);

    /* 6. Open FIFO Write End (AFTER fork) */
    ret = logger_open_write_fifo();
    if (ret != GATEWAY_SUCCESS) {
        fprintf(stderr,"CRITICAL: Main process failed to open FIFO write end (Error %d). Terminating child and exiting.\n", ret);
        kill(log_pid, SIGTERM);
        waitpid(log_pid, NULL, 0);
        #ifdef DATAMGT_H
        datamgt_free_room_sensor_map(&room_map);
        #endif
        logger_cleanup();
        return EXIT_FAILURE;
    }
    /* Now logging via log_message is safe */
    log_message(LOG_LEVEL_INFO, "Main process logger FIFO opened successfully."); 
    log_message(LOG_LEVEL_INFO, "Main process PID: %d, Log process PID: %d", getpid(), log_pid); 

    #ifdef DATAMGT_H
    if (room_map != NULL) {
        log_message(LOG_LEVEL_INFO, "Room sensor map '%s' loaded (%d entries).", map_filename, room_map->count); 
    } else {
        log_message(LOG_LEVEL_WARNING, "Room sensor map '%s' failed to load or was empty.", map_filename); 
    }
    #endif

    /* 7. Initialize Shared Buffer */
    ret = sbuffer_init(&buffer);
    if (ret != GATEWAY_SUCCESS || buffer == NULL) {
        log_message(LOG_LEVEL_FATAL, "Failed to initialize shared buffer (Error %d). Terminating.", ret); 
        kill(log_pid, SIGTERM);
        waitpid(log_pid, NULL, 0);
        #ifdef DATAMGT_H
        datamgt_free_room_sensor_map(&room_map);
        #endif
        logger_cleanup();
        return EXIT_FAILURE;
    }
    log_message(LOG_LEVEL_INFO, "Shared buffer initialized."); 

    /* 8. Set up Signal Handler */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler; // Simple flag setter
    // Register for SIGUSR1 if used for other purposes, but main loop uses sigwaitinfo
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        log_message(LOG_LEVEL_WARNING, "Failed to register SIGUSR1 handler: %s.", strerror(errno)); 
        // Continue anyway as sigwaitinfo is primary
    }
    log_message(LOG_LEVEL_INFO, "Signal mask set, main thread will use sigwaitinfo() to wait for termination signals."); 


    /* 9. Prepare Thread Arguments */
    #ifdef CONMGT_H
    memset(&conmgt_args, 0, sizeof(conmgt_args));
    conmgt_args.server_port = server_port;
    conmgt_args.buffer = buffer;
    #endif
    #ifdef DATAMGT_H
    memset(&datamgt_args, 0, sizeof(datamgt_args));
    datamgt_args.buffer = buffer;
    datamgt_args.map = room_map;
    #endif
    #ifdef STORAGEMGT_H
    memset(&storagemgt_args, 0, sizeof(storagemgt_args));
    storagemgt_args.buffer = buffer;
    #endif

    /* 10. Create Manager Threads */
    log_message(LOG_LEVEL_INFO, "Creating manager threads..."); 
    #ifdef CONMGT_H
    if (pthread_create(&conmgt_thread_id, NULL, conmgt_run, &conmgt_args) == 0) {
        conmgt_created = true; log_message(LOG_LEVEL_DEBUG, "Connection Manager thread created (ID: %lu).", (unsigned long)conmgt_thread_id); 
    } else {
        log_message(LOG_LEVEL_FATAL, "Failed to create Connection thread: %s", strerror(errno)); 
        goto immediate_cleanup_on_create_fail;
    }
    #endif

    #ifdef DATAMGT_H
    if (pthread_create(&datamgt_thread_id, NULL, datamgt_run, &datamgt_args) == 0) {
        datamgt_created = true; log_message(LOG_LEVEL_DEBUG, "Data Manager thread created (ID: %lu).", (unsigned long)datamgt_thread_id); 
    } else {
        log_message(LOG_LEVEL_FATAL, "Failed to create Data thread: %s", strerror(errno)); 
        goto immediate_cleanup_on_create_fail;
    }
    #endif

    #ifdef STORAGEMGT_H
    if (pthread_create(&storagemgt_thread_id, NULL, storagemgt_run, &storagemgt_args) == 0) {
        storagemgt_created = true; log_message(LOG_LEVEL_DEBUG, "Storage Manager thread created (ID: %lu).", (unsigned long)storagemgt_thread_id); 
    } else {
        log_message(LOG_LEVEL_FATAL, "Failed to create Storage thread: %s", strerror(errno)); 
        goto immediate_cleanup_on_create_fail;
    }
    #endif
    log_message(LOG_LEVEL_INFO, "All manager threads created successfully."); 

    #ifdef CMDIF_H
    log_message(LOG_LEVEL_INFO, "Creating command interface thread..."); 
    if (pthread_create(&cmdif_thread_id, NULL, cmdif_run, &cmdif_args) == 0) {
        cmdif_created = true;
        log_message(LOG_LEVEL_INFO, "Command interface thread created (ID: %lu).", (unsigned long)cmdif_thread_id); 
    } else {
        log_message(LOG_LEVEL_FATAL, "Failed to create Command Interface thread: %s", strerror(errno)); 
        goto immediate_cleanup_on_create_fail;
    }
    #endif

    /* 11. Wait for Termination Signal (Blocking Call) */
    log_message(LOG_LEVEL_INFO, "Main thread waiting for termination signal (SIGINT/SIGTERM)..."); 
    printf("INFO: Gateway running. Press Ctrl+C to stop.\n"); 

    errno = 0;
    signum_received = sigwaitinfo(&wait_mask, NULL);

    if (signum_received == -1) {
        log_message(LOG_LEVEL_ERROR, "sigwaitinfo failed: %s. Initiating cleanup anyway.", strerror(errno)); 
        terminate_flag = 1; /* Ensure flag is set for cleanup logic */
    } else {
        log_message(LOG_LEVEL_INFO, "Main thread received shutdown signal (%s). Initiating shutdown...", strsignal(signum_received)); 
        terminate_flag = 1; /* Set flag based on signal */
        printf("\nINFO: Shutdown signal received. Shutting down...\n"); 
    }

/* Cleanup section - reached after signal or thread creation failure */
immediate_cleanup_on_create_fail:
    log_message(LOG_LEVEL_INFO, "Main process initiating cleanup sequence..."); 
    fprintf(stderr, "INFO: Main process initiating cleanup sequence...\n"); 

    /* 12. Initiate Graceful Shutdown Sequence */
    #ifdef CONMGT_H
    if (conmgt_created) { 
        conmgt_stop(); 
        fprintf(stderr, "INFO: Connection Manager stop signaled.\n"); 
    }
    #endif
    #ifdef CMDIF_H
    if (cmdif_created) { 
        cmdif_stop(); 
        fprintf(stderr, "INFO: Command Interface stop signaled.\n"); 
    }
    #endif
    #ifdef DATAMGT_H
    if (datamgt_created) { 
        datamgt_stop(); 
        fprintf(stderr, "INFO: Data Manager stop requested.\n"); 
    }
    #endif
    #ifdef STORAGEMGT_H
    if (storagemgt_created) { 
        storagemgt_stop(); 
        fprintf(stderr, "INFO: Storage Manager stop requested.\n"); 
    }
    #endif

    if (buffer != NULL) {
        log_message(LOG_LEVEL_INFO, "Signaling shared buffer shutdown..."); 
        fprintf(stderr, "INFO: Signaling shared buffer shutdown...\n");
        sbuffer_signal_shutdown(buffer);
    }

    /* 13. Join threads */
    log_message(LOG_LEVEL_INFO, "Joining threads after signaling stop..."); 
    fprintf(stderr, "INFO: Joining threads...\n"); 

    #ifdef CMDIF_H
    if (cmdif_created) { 
        if (pthread_join(cmdif_thread_id, &thread_result) != 0) { 
            log_message(LOG_LEVEL_WARNING, "Failed to join Command Interface thread: %s", strerror(errno)); 
        } else { 
            log_message(LOG_LEVEL_INFO, "Command Interface thread joined."); 
            fprintf(stderr, "INFO: Command Interface thread joined.\n"); 
        } 
    }
    #endif
    #ifdef STORAGEMGT_H
    if (storagemgt_created) { 
        if (pthread_join(storagemgt_thread_id, &thread_result) != 0) { 
            log_message(LOG_LEVEL_WARNING, "Failed to join Storage Manager thread: %s", strerror(errno)); 
        } else { 
            log_message(LOG_LEVEL_INFO, "Storage Manager thread joined."); 
            fprintf(stderr, "INFO: Storage Manager thread joined.\n"); 
        } 
    }
    #endif
    #ifdef DATAMGT_H
    if (datamgt_created) { 
        if (pthread_join(datamgt_thread_id, &thread_result) != 0) { 
            log_message(LOG_LEVEL_WARNING, "Failed to join Data Manager thread: %s", strerror(errno)); 
        } else { 
            log_message(LOG_LEVEL_INFO, "Data Manager thread joined."); 
            fprintf(stderr, "INFO: Data Manager thread joined.\n"); 
        } 
    }
    #endif
    #ifdef CONMGT_H
    if (conmgt_created) { 
        if (pthread_join(conmgt_thread_id, &thread_result) != 0) { 
            log_message(LOG_LEVEL_WARNING, "Failed to join Connection Manager thread: %s", strerror(errno)); 
        } else { 
            log_message(LOG_LEVEL_INFO, "Connection Manager thread joined."); 
            fprintf(stderr, "INFO: Connection Manager thread joined.\n"); 
        } 
    }
    #endif
    log_message(LOG_LEVEL_INFO, "Finished joining manager threads."); 


    /* 14. Cleanup Shared Resources */
    fprintf(stderr, "INFO: Cleaning up shared resources...\n"); 
    #ifdef DATAMGT_H
    datamgt_free_room_sensor_map(&room_map); // Logs internally
    #endif
    if (buffer != NULL) {
        ret = sbuffer_free(&buffer);
        if (ret != GATEWAY_SUCCESS) { log_message(LOG_LEVEL_WARNING, "Error during sbuffer free (Code: %d).", ret); fprintf(stderr, "WARN: Error during sbuffer free.\n"); }
        else { log_message(LOG_LEVEL_INFO, "Shared buffer freed."); fprintf(stderr, "INFO: Shared buffer freed.\n"); }
    }

    /* 15. Clean up Logger */
    logger_cleanup(); // Logs to stderr internally
    fprintf(stderr, "INFO: Logger resources cleaned up.\n"); 

    /* 16. Wait for Log Process to Exit */
    fprintf(stderr, "INFO: Waiting for log process (PID: %d) to exit...\n", log_pid); 
    int log_status;
    if (log_pid > 0) {
        if (waitpid(log_pid, &log_status, 0) == -1) {
            if (errno != ECHILD) { 
                perror("WARN: waitpid() for log process failed"); 
            }
            else { 
                fprintf(stderr, "INFO: Log process had already exited.\n"); 
            }
        } else {
            if (WIFEXITED(log_status)) { 
                fprintf(stderr, "INFO: Log process exited with status %d.\n", WEXITSTATUS(log_status)); 
            }
            else if (WIFSIGNALED(log_status)) { 
                fprintf(stderr, "INFO: Log process terminated by signal %d (%s).\n", WTERMSIG(log_status), strsignal(WTERMSIG(log_status))); 
            }
            else { 
                fprintf(stderr, "INFO: Log process exited with unknown status.\n"); 
            }
        }
    }

    fprintf(stderr, "INFO: Sensor gateway finished shutting down.\n");

    return (terminate_flag ? EXIT_FAILURE : EXIT_SUCCESS);

} /* End of main */


/* --- Function Definitions --- */

/**
 * @brief Prints command line usage instructions.
 */
static void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <port>\n", prog_name);
    fprintf(stderr, "  <port>: The TCP port number to listen on (%d-%d)\n", MIN_PORT, MAX_PORT);
}

/**
 * @brief Signal handler for user signals.
 * IMPORTANT: Only use async-signal-safe functions inside!
 */
static void signal_handler(int sig) {
    /* User Signal handler */
    (void)sig; /* Suppress unused parameter warning */
}