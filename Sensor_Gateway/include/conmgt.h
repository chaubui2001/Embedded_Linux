#ifndef CONMGT_H
#define CONMGT_H

#include <arpa/inet.h>
#include <time.h>
#include <stddef.h>

/* Include project-specific headers */
#include "sbuffer.h" /* Required for sbuffer_t */
#include "common.h"  /* Required for gateway_error_t */

/* Structure to pass arguments to the connection manager thread */
typedef struct {
    int server_port;   /* The TCP port number to listen on */
    sbuffer_t *buffer; /* Pointer to the shared buffer */
} conmgt_args_t;

/* Structure to hold information about each connected client */
typedef struct {
    int socket_fd;
    sensor_id_t sensor_id;
    time_t last_active_ts;
    bool id_received;
    char client_ip[INET_ADDRSTRLEN]; /* Store client IP address string */
    int client_port;                 /* Store client port number */
    time_t connection_start_ts;      /* Store connection start timestamp */
} client_info_t;

/**
* The main function for the Connection Manager thread.
* Sets up the server socket, listens for incoming TCP connections, 
* accepts clients, reads sensor data, and inserts it into the shared buffer.
* Manages active connections and handles timeouts.
* @param arg A pointer to a connmgr_args_t struct containing thread arguments.
* @return Always returns NULL. Errors should be handled internally or logged.
*/
void *conmgt_run(void *arg);

/**
* @brief Signals the Connection Manager thread to stop gracefully.
* Writes to a shutdown pipe to interrupt the select() loop.
*/
void conmgt_stop(void);

/**
 * @brief Gathers connection statistics for all active clients.
 * Formats the information into the provided buffer. Thread-safe.
 * @param buffer The output buffer to store the formatted statistics.
 * @param size The maximum size of the output buffer.
 * @return The number of active connections formatted, or -1 on error.
 */
int conmgt_get_connection_stats(char *buffer, size_t size);

 /**
  * @brief Gets the current number of active client connections. Thread-safe.
  * @return The number of active connections.
  */
int conmgt_get_active_connections();

#endif /* CONMGT_H */