/* Include standard libraries */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>

/* Include project-specific headers */
#include "config.h"
#include "common.h"     // Assumes original client_info_t (no SSL*)
#include "sbuffer.h"
#include "logger.h"     // Assumes updated logger.h
#include "conmgt.h"

/* --- Local Macros --- */
#define MAX_CONNECTIONS 100       /* Max simultaneous client connections */
#define SELECT_TIMEOUT_SEC 1        /* Timeout for select() in seconds */
#define EXPECTED_PACKET_SIZE (sizeof(uint16_t) + sizeof(double)) /* Expected size of data packet */

/* --- Static Variables (Module State) --- */
static client_info_t client_sockets[MAX_CONNECTIONS]; /* Array to store client info */
static int num_clients = 0;                           /* Number of active clients */
static int server_sd = -1;                            /* Server socket descriptor */
static int shutdown_pipe_fd[2] = {-1, -1};            /* Pipe for shutdown signal: [0]=read, [1]=write */
static volatile bool stop_requested = false;          /* Flag to prevent multiple stop actions */
pthread_mutex_t conmgt_mutex; /* Mutex for thread safety in conmgt */

/* --- Forward Declarations (Internal Helper Functions) --- */

static gateway_error_t setup_server_socket(int port);
static void handle_new_connection(int *max_sd);
static void handle_client_data(int client_index, fd_set *read_fds, sbuffer_t *buffer);
static void check_sensor_timeouts(fd_set *read_fds);
static void add_client(int client_sd, struct sockaddr_in *client_addr);
static void remove_client(int client_index, fd_set *read_fds);

/* --- Main Thread Function Implementation --- */

void *conmgt_run(void *arg) {
    conmgt_args_t *args = (conmgt_args_t *)arg;
    sbuffer_t *buffer = args->buffer;
    int max_sd = -1;
    fd_set read_fds;
    struct timeval select_timeout;
    gateway_error_t ret;
    int activity;
    bool running = true; /* Loop control flag */

    stop_requested = false; /* Reset flag on start */

    /* Initialize client socket array */
    for (int i = 0; i < MAX_CONNECTIONS; ++i) {
        client_sockets[i].socket_fd = -1;
        client_sockets[i].id_received = false;
    }

    /* 1. Create Shutdown Pipe */
    if (pipe(shutdown_pipe_fd) == -1) {
        log_message(LOG_LEVEL_FATAL, "Connection manager failed to create shutdown pipe: %s. Exiting thread.", strerror(errno)); 
        return NULL;
    }
    fcntl(shutdown_pipe_fd[0], F_SETFL, O_NONBLOCK);

    /* 2. Setup the server socket (uses static server_sd) */
    ret = setup_server_socket(args->server_port); // setup_server_socket logs internally
    if (ret != GATEWAY_SUCCESS) {
        log_message(LOG_LEVEL_FATAL, "Connection manager failed to set up server socket (Error code: %d). Exiting thread.", ret); 
        close(shutdown_pipe_fd[0]); /* Close pipe fds on error */
        close(shutdown_pipe_fd[1]);
        return NULL;
    }
    /* Initialize max_sd considering server and pipe */
    // Calculate max_sd based on server_sd and shutdown_pipe_fd initially
    if (server_sd != -1 && shutdown_pipe_fd[0] != -1) {
         max_sd = (server_sd > shutdown_pipe_fd[0]) ? server_sd : shutdown_pipe_fd[0];
    } else if (server_sd != -1) {
        max_sd = server_sd;
    } else if (shutdown_pipe_fd[0] != -1) {
        max_sd = shutdown_pipe_fd[0];
    } else {
         log_message(LOG_LEVEL_FATAL, "Neither server socket nor shutdown pipe is valid after setup. Exiting thread.");
         return NULL; /* Should not happen if setup succeeded */
    }

    log_message(LOG_LEVEL_INFO, "Server socket listening on port %d", args->server_port); 

    /* 3. Main select() loop */
    while (running) {
        FD_ZERO(&read_fds);
        /* Add server socket if it's still open */
        if (server_sd != -1) {
            FD_SET(server_sd, &read_fds);
        }
        /* Add shutdown pipe read end */
        if (shutdown_pipe_fd[0] != -1) {
            FD_SET(shutdown_pipe_fd[0], &read_fds);
        }

        /* Reset max_sd for this iteration */
        // Recalculate max_sd based on current valid FDs
        if (server_sd != -1 && shutdown_pipe_fd[0] != -1) {
             max_sd = (server_sd > shutdown_pipe_fd[0]) ? server_sd : shutdown_pipe_fd[0];
        } else if (server_sd != -1) {
            max_sd = server_sd;
        } else if (shutdown_pipe_fd[0] != -1) {
            max_sd = shutdown_pipe_fd[0];
        } else {
             log_message(LOG_LEVEL_DEBUG, "No active listeners (server socket or pipe closed). Exiting conmgt loop.");
             break;
        }

        /* Add active client sockets and update max_sd */
        for (int i = 0; i < num_clients; ++i) {
            if (client_sockets[i].socket_fd != -1) {
                FD_SET(client_sockets[i].socket_fd, &read_fds);
                if (client_sockets[i].socket_fd > max_sd) {
                    max_sd = client_sockets[i].socket_fd;
                }
            }
        }

        select_timeout.tv_sec = SELECT_TIMEOUT_SEC;
        select_timeout.tv_usec = 0;

        activity = select(max_sd + 1, &read_fds, NULL, NULL, &select_timeout);

        if (activity < 0) {
            if (errno == EINTR) { /* Interrupted system call, possibly by our shutdown signal */
                 log_message(LOG_LEVEL_DEBUG,"select() interrupted, likely by signal or timeout handling."); 
                continue; /* Re-check loop condition */
            } else {
                 log_message(LOG_LEVEL_ERROR, "select() failed: %s", strerror(errno)); 
                sleep(1); /* Avoid busy-looping */
                continue;
            }
        }

        /* Check if shutdown was requested via pipe */
        if (shutdown_pipe_fd[0] != -1 && FD_ISSET(shutdown_pipe_fd[0], &read_fds)) {
            char dummy_buffer[1];
            read(shutdown_pipe_fd[0], dummy_buffer, 1); /* Read the byte */
            log_message(LOG_LEVEL_INFO,"Shutdown signal received via pipe. Stopping connection manager loop."); 
            running = false; /* Set flag to break loop */
            continue; /* Go to top of loop to exit */
        }

        /* Check for new connections (only if server socket is still active) */
        if (server_sd != -1 && FD_ISSET(server_sd, &read_fds)) {
            handle_new_connection(&max_sd);
        }

        /* Check client activity - Iterate backwards for safe removal */
        for (int i = num_clients - 1; i >= 0; --i) {
            /* Check if socket still exists and has activity */
            if (client_sockets[i].socket_fd != -1 && FD_ISSET(client_sockets[i].socket_fd, &read_fds)) {
                handle_client_data(i, &read_fds, buffer);
            }
        }

        /* Check timeouts (only if not shutting down) */
        if (running) {
            check_sensor_timeouts(&read_fds);
        }

    } /* End of main while loop */

    /* --- Cleanup --- */
    log_message(LOG_LEVEL_INFO, "Connection manager shutting down..."); 

    /* Ensure server socket is closed */
    if (server_sd != -1) {
        close(server_sd);
        server_sd = -1;
        log_message(LOG_LEVEL_DEBUG, "Server socket closed during cleanup."); 
    }

    /* Close remaining client sockets */
    log_message(LOG_LEVEL_INFO, "Closing remaining client connections..."); 
    for (int i = 0; i < MAX_CONNECTIONS; ++i) {
        if (client_sockets[i].socket_fd != -1) {
             if (client_sockets[i].id_received) {
                 log_message(LOG_LEVEL_INFO, "Closing connection for sensor %d (socket %d) during shutdown.", 
                             client_sockets[i].sensor_id, client_sockets[i].socket_fd);
             } else {
                  log_message(LOG_LEVEL_INFO, "Closing connection for unidentified client (socket %d) during shutdown.", 
                             client_sockets[i].socket_fd);
             }
            close(client_sockets[i].socket_fd);
            client_sockets[i].socket_fd = -1;
        }
    }
    num_clients = 0;

    /* Close pipe descriptors */
    if (shutdown_pipe_fd[0] != -1) close(shutdown_pipe_fd[0]);
    if (shutdown_pipe_fd[1] != -1) close(shutdown_pipe_fd[1]);
    shutdown_pipe_fd[0] = shutdown_pipe_fd[1] = -1;
    log_message(LOG_LEVEL_DEBUG, "Shutdown pipe closed during cleanup."); 

    log_message(LOG_LEVEL_INFO, "Connection manager finished cleanup."); 
    return NULL;
}

/**
 * @brief Signals the Connection Manager thread to stop gracefully.
 * Closes the server socket and writes to a shutdown pipe. Thread-safe check.
 */
void conmgt_stop(void) {
    char dummy = 's';
    bool already_stopping = __sync_bool_compare_and_swap(&stop_requested, false, true);

    if (already_stopping) {
        log_message(LOG_LEVEL_INFO, "Initiating Connection Manager shutdown sequence..."); 

        /* Close server socket immediately */
        if (server_sd != -1) {
            log_message(LOG_LEVEL_INFO, "Closing server socket to stop new connections."); 
            if(close(server_sd) == -1){
                 log_message(LOG_LEVEL_WARNING, "Error closing server socket: %s", strerror(errno)); 
            }
            server_sd = -1; /* Mark as closed */
        } else {
            log_message(LOG_LEVEL_INFO, "Server socket already closed or not initialized."); 
        }

        /* Write to the shutdown pipe */
        if (shutdown_pipe_fd[1] != -1) {
            if (write(shutdown_pipe_fd[1], &dummy, 1) == -1) {
                if (errno != EPIPE) {
                    log_message(LOG_LEVEL_ERROR, "Failed to write to shutdown pipe: %s", strerror(errno)); 
                } else {
                    log_message(LOG_LEVEL_INFO, "Shutdown pipe read end already closed."); 
                }
            } else {
                log_message(LOG_LEVEL_INFO, "Shutdown signal sent to Connection Manager thread via pipe."); 
            }
        } else {
            log_message(LOG_LEVEL_WARNING, "Shutdown pipe write end is invalid. Cannot signal thread via pipe."); 
        }
    } else {
        log_message(LOG_LEVEL_INFO, "Connection Manager shutdown already in progress or completed."); 
    }
}


/* --- Implementation of internal helper functions --- */

/**
 * @brief Sets up the server listening socket. (Uses static server_sd)
 * @param port The port number to listen on.
 * @return GATEWAY_SUCCESS on success, error code otherwise.
 */
static gateway_error_t setup_server_socket(int port) {
    struct sockaddr_in server_addr;
    int opt = 1;
    // char log_buf[LOG_BUFFER_SIZE]; // No longer needed

    if ((server_sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        log_message(LOG_LEVEL_ERROR, "Failed to create server socket: %s", strerror(errno)); 
        return CONNMGR_SOCKET_CREATE_ERR;
    }

    if (setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_message(LOG_LEVEL_ERROR, "setsockopt(SO_REUSEADDR) failed: %s", strerror(errno)); 
        close(server_sd);
        server_sd = -1;
        return CONNMGR_ERROR; // Generic error
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(server_sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to bind server socket to port %d: %s", port, strerror(errno)); 
        close(server_sd);
        server_sd = -1;
        return CONNMGR_SOCKET_BIND_ERR;
    }

    if (listen(server_sd, TCP_BACKLOG) < 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to listen on server socket: %s", strerror(errno)); 
        close(server_sd);
        server_sd = -1;
        return CONNMGR_SOCKET_LISTEN_ERR;
    }

    // Log success implicitly handled by caller log
    return GATEWAY_SUCCESS;
}

/**
 * @brief Handles a new incoming connection request. (Uses static server_sd)
 * @param max_sd Pointer to the maximum socket descriptor value (will be updated).
 */
static void handle_new_connection(int *max_sd) {
    int client_sd;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int current_connections_from_ip = 0; /* Counter for connections from this IP */
    char client_ip_str[INET_ADDRSTRLEN]; /* Buffer to hold incoming IP string */

    if (server_sd == -1) {
        return;
    }

    client_sd = accept(server_sd, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_sd < 0) {
        if (errno != EBADF && errno != EINVAL) { /* EBADF occurs if server_sd closed between select() and accept() */
            log_message(LOG_LEVEL_ERROR, "accept() failed: %s", strerror(errno)); 
        }
        return;
    }

    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip_str, INET_ADDRSTRLEN);

    /* --- BEGIN: Connection Limiting --- */
    #ifdef MAX_CONNECTIONS_PER_IP
    pthread_mutex_lock(&conmgt_mutex); // Assume mutex protects client_sockets
    for (int i = 0; i < MAX_CONNECTIONS; ++i) {
        if (client_sockets[i].socket_fd != -1 && client_sockets[i].client_ip[0] != '\0' &&
            strcmp(client_sockets[i].client_ip, client_ip_str) == 0) {
            current_connections_from_ip++;
        }
    }
    pthread_mutex_unlock(&conmgt_mutex);

    if (current_connections_from_ip >= MAX_CONNECTIONS_PER_IP) {
        log_message(LOG_LEVEL_WARNING, "Connection limit (%d) reached for IP %s. Rejecting new connection (socket %d).", 
                    MAX_CONNECTIONS_PER_IP, client_ip_str, client_sd);
        close(client_sd); /* Close the rejected socket */
        return; /* Stop processing this new connection */
    }
    #endif
    /* --- END: Connection Limiting --- */

    log_message(LOG_LEVEL_INFO, "New connection accepted from %s:%d (socket %d). Current connections from this IP: %d", 
                client_ip_str, ntohs(client_addr.sin_port), client_sd, current_connections_from_ip);

    /* Add the client */
    pthread_mutex_lock(&conmgt_mutex); // Assume mutex protects add_client
    add_client(client_sd, &client_addr); // add_client logs internally
    pthread_mutex_unlock(&conmgt_mutex);


    if (client_sd > *max_sd) {
        *max_sd = client_sd;
    }
}

/**
 * @brief Handles incoming data from a specific client. Reads data, parses it,
 * updates timestamp, inserts into buffer, and handles disconnections/errors.
 * @param client_index The index of the client in the client_sockets array.
 * @param read_fds The set of read descriptors (needed by remove_client).
 * @param buffer Pointer to the shared buffer.
 */
 static void handle_client_data(int client_index, fd_set *read_fds, sbuffer_t *buffer) {
    /* Ensure client_index is valid */
    if (client_index < 0 || client_index >= MAX_CONNECTIONS || client_sockets[client_index].socket_fd == -1) {
        return; 
    }

    int client_sd = client_sockets[client_index].socket_fd;
    char recv_buffer[EXPECTED_PACKET_SIZE];
    ssize_t bytes_received;
    sensor_data_t sensor_reading;
    gateway_error_t sbuf_ret;

    bytes_received = read(client_sd, recv_buffer, EXPECTED_PACKET_SIZE);

    /* 1. Check for read errors */
    if (bytes_received < 0) {
        log_message(LOG_LEVEL_ERROR, "read() failed for socket %d: %s", client_sd, strerror(errno)); 
        if (client_sockets[client_index].id_received) {
             log_message(LOG_LEVEL_INFO, "Closing connection due to read error for sensor %d (socket %d)", 
                         client_sockets[client_index].sensor_id, client_sd);
        } else {
             log_message(LOG_LEVEL_INFO, "Closing connection due to read error before ID received (socket %d)", client_sd); 
        }
        pthread_mutex_lock(&conmgt_mutex); // Protect remove_client
        remove_client(client_index, read_fds);
        pthread_mutex_unlock(&conmgt_mutex);
    }
    /* 2. Check for connection closed by client (EOF) */
    else if (bytes_received == 0) {
        if (client_sockets[client_index].id_received) {
            log_message(LOG_LEVEL_INFO, "Sensor node %d has closed the connection (socket %d)", 
                        client_sockets[client_index].sensor_id, client_sd);
       } else {
            log_message(LOG_LEVEL_INFO, "Connection closed by client before sending ID (socket %d)", client_sd); 
       }
        pthread_mutex_lock(&conmgt_mutex); // Protect remove_client
        remove_client(client_index, read_fds);
        pthread_mutex_unlock(&conmgt_mutex);
    }
    /* 3. Check if the expected number of bytes were received */
    else if ((size_t)bytes_received == EXPECTED_PACKET_SIZE) {
        /* Data received successfully */
        uint16_t network_sensor_id;
        double network_value;

        memcpy(&network_sensor_id, recv_buffer, sizeof(uint16_t));
        memcpy(&network_value, recv_buffer + sizeof(uint16_t), sizeof(double));

        sensor_reading.id = ntohs(network_sensor_id);
        sensor_reading.value = network_value;
        sensor_reading.ts = time(NULL);

        client_sockets[client_index].last_active_ts = sensor_reading.ts;

        if (!client_sockets[client_index].id_received) {
            client_sockets[client_index].sensor_id = sensor_reading.id;
            client_sockets[client_index].id_received = true;
            log_message(LOG_LEVEL_INFO, "Sensor node %d has opened a new connection (socket %d)", 
                         sensor_reading.id, client_sd);
        } else if (client_sockets[client_index].sensor_id != sensor_reading.id) {
            log_message(LOG_LEVEL_WARNING, "Sensor ID changed on socket %d from %d to %d", 
                         client_sd, client_sockets[client_index].sensor_id, sensor_reading.id);
            client_sockets[client_index].sensor_id = sensor_reading.id; // Update ID
        }

        sbuf_ret = sbuffer_insert(buffer, &sensor_reading);
        if (sbuf_ret != GATEWAY_SUCCESS) {
            log_message(LOG_LEVEL_ERROR, "Failed to insert data from sensor %d into buffer (Error %d)", 
                         sensor_reading.id, sbuf_ret);
        } else {
             log_message(LOG_LEVEL_DEBUG, "Sensor %d data inserted into buffer (socket %d)", 
                           sensor_reading.id, client_sd);
       }

    }
    /* 4. Handle partial or unexpected data size */
    else {
         log_message(LOG_LEVEL_WARNING, "Received partial/unexpected data size (%zd bytes, expected %zu) from socket %d. Closing connection.", 
                     bytes_received, EXPECTED_PACKET_SIZE, client_sd);
        if (client_sockets[client_index].id_received) {
             log_message(LOG_LEVEL_INFO, "Closing connection due to partial read for sensor %d (socket %d)", 
                         client_sockets[client_index].sensor_id, client_sd);
        } else {
             log_message(LOG_LEVEL_INFO, "Closing connection due to partial read before ID received (socket %d)", client_sd); 
        }
        pthread_mutex_lock(&conmgt_mutex); // Protect remove_client
        remove_client(client_index, read_fds);
        pthread_mutex_unlock(&conmgt_mutex);
    }
}


/**
 * @brief Checks all active clients for inactivity timeouts.
 * @param read_fds The set of read descriptors (will be modified by remove_client).
 */
static void check_sensor_timeouts(fd_set *read_fds) {
    time_t now = time(NULL);

    pthread_mutex_lock(&conmgt_mutex); // Protect access
    /* Iterate backwards for safe removal */
    for (int i = num_clients - 1; i >= 0; --i) { // Iterate based on actual client count
        if (client_sockets[i].socket_fd != -1) { // Check if slot is active
            if ((now - client_sockets[i].last_active_ts) > SENSOR_TIMEOUT_SEC) {
                if (client_sockets[i].id_received) {
                     log_message(LOG_LEVEL_INFO, "Sensor node %d timed out (socket %d). Closing connection.", 
                                 client_sockets[i].sensor_id, client_sockets[i].socket_fd);
                } else {
                     log_message(LOG_LEVEL_INFO, "Client timed out before sending ID (socket %d). Closing connection.", 
                                 client_sockets[i].socket_fd);
                }
                remove_client(i, read_fds); // remove_client logs the removal details
            }
        }
    }
    pthread_mutex_unlock(&conmgt_mutex);
}

/**
 * @brief Adds a new client to the client_sockets array.
 * @param client_sd The socket descriptor of the new client.
 * @param client_addr The address structure of the new client.
 */
static void add_client(int client_sd, struct sockaddr_in *client_addr) {

    if (num_clients >= MAX_CONNECTIONS) {
        log_message(LOG_LEVEL_WARNING, "Maximum connection limit (%d) reached. Rejecting connection from %s:%d", 
                     MAX_CONNECTIONS, inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
        close(client_sd);
        return;
    }

    /* Find the first available slot */
    int i;
    for (i = 0; i < MAX_CONNECTIONS; ++i) {
        if (client_sockets[i].socket_fd == -1) {
            client_sockets[i].socket_fd = client_sd;
            client_sockets[i].last_active_ts = time(NULL);
            client_sockets[i].id_received = false;
            client_sockets[i].sensor_id = 0;
            client_sockets[i].connection_start_ts = client_sockets[i].last_active_ts;

            inet_ntop(AF_INET, &(client_addr->sin_addr), client_sockets[i].client_ip, INET_ADDRSTRLEN);
            client_sockets[i].client_port = ntohs(client_addr->sin_port);

            num_clients++;
            log_message(LOG_LEVEL_DEBUG, "Added client %s:%d (socket %d) at index %d. Total clients: %d", 
                        client_sockets[i].client_ip, client_sockets[i].client_port, client_sd, i, num_clients);
            return;
        }
    }
    log_message(LOG_LEVEL_ERROR, "Internal Error: Could not find slot to add client socket %d.", client_sd); 
    close(client_sd);
}

/**
 * @brief Removes a client from the list and closes its socket.
 * Uses compaction by moving the last element.
 * @param client_index The index of the client to remove.
 * @param read_fds The set of read descriptors to potentially remove the fd from.
 */
static void remove_client(int client_index, fd_set *read_fds) {

    if (client_index < 0 || client_index >= MAX_CONNECTIONS || client_sockets[client_index].socket_fd == -1) {
        return; /* Invalid index or slot already processed */
    }

    int client_sd = client_sockets[client_index].socket_fd;

    /* Remove from select set (if fd is valid) */
    if (client_sd >= 0) {
         FD_CLR(client_sd, read_fds);
         close(client_sd);
    }

    log_message(LOG_LEVEL_DEBUG, "Removing client index %d (socket %d, ID: %u). Current count %d.", 
                 client_index, client_sd, client_sockets[client_index].id_received ? client_sockets[client_index].sensor_id : 0, num_clients);

    if (client_sd >= 0) { FD_CLR(client_sd, read_fds); close(client_sd); }

    client_sockets[client_index].socket_fd = -1;
    client_sockets[client_index].id_received = false;
    client_sockets[client_index].client_ip[0] = '\0';
    client_sockets[client_index].sensor_id = 0;


    /* Compaction */
    int last_valid_index = -1;
    for(int j = num_clients - 1; j >= 0; --j) { if(client_sockets[j].socket_fd != -1) { last_valid_index = j; break; } }
    if (last_valid_index != -1 && client_index < last_valid_index) {
        log_message(LOG_LEVEL_DEBUG, "Compacting: Moving client from index %d to %d.", last_valid_index, client_index); 
        client_sockets[client_index] = client_sockets[last_valid_index];
        client_sockets[last_valid_index].socket_fd = -1;
        client_sockets[last_valid_index].id_received = false;
        client_sockets[last_valid_index].client_ip[0] = '\0';
        client_sockets[last_valid_index].sensor_id = 0;
    } else if (last_valid_index == client_index) {
        log_message(LOG_LEVEL_DEBUG, "Removed last active client index %d.", client_index); 
    }

    num_clients--;
    log_message(LOG_LEVEL_DEBUG, "Client removed. New client count: %d.", num_clients); 
}

/**
 * @brief Gathers connection statistics for all active clients.
 * Formats the information into the provided buffer. Thread-safe.
 * @param buffer The output buffer to store the formatted statistics.
 * @param size The maximum size of the output buffer.
 * @return The number of active connections formatted, or -1 on error (e.g., buffer too small).
 */
 int conmgt_get_connection_stats(char *output_buffer, size_t buffer_size) {
    if (output_buffer == NULL || buffer_size == 0) {
        return -1;
    }

    char line_buffer[256];
    size_t current_len = 0;
    int connections_found = 0;
    time_t now = time(NULL);

    pthread_mutex_lock(&conmgt_mutex);

    current_len += snprintf(output_buffer + current_len, buffer_size - current_len,
                            "--- Active Connections (%d) ---\n", num_clients);
    if (current_len >= buffer_size) goto buffer_full;

    for (int i = 0; i < MAX_CONNECTIONS; ++i) {
        if (client_sockets[i].socket_fd != -1) {
            connections_found++;
            time_t connected_duration = now - client_sockets[i].connection_start_ts;
            int hours = connected_duration / 3600;
            int mins = (connected_duration % 3600) / 60;
            int secs = connected_duration % 60;

            int len = snprintf(line_buffer, sizeof(line_buffer),
                                 "  Sensor ID: %-5u | IP: %-15s | Port: %-5d | Socket: %-3d | Connected: %02d:%02d:%02d\n",
                                 client_sockets[i].id_received ? client_sockets[i].sensor_id : 0,
                                 client_sockets[i].client_ip,
                                 client_sockets[i].client_port,
                                 client_sockets[i].socket_fd,
                                 hours, mins, secs);

            if (current_len + len >= buffer_size) {
                goto buffer_full; /* Not enough space for this line */
            }
            memcpy(output_buffer + current_len, line_buffer, len);
            current_len += len;
        }
    }

    if (current_len < buffer_size) {
        output_buffer[current_len] = '\0'; /* Null-terminate */
    } else {
        goto buffer_full; /* No space even for null terminator */
    }

    pthread_mutex_unlock(&conmgt_mutex);
    return connections_found;

buffer_full:
    pthread_mutex_unlock(&conmgt_mutex);
    if (buffer_size > 0) output_buffer[buffer_size - 1] = '\0';
    log_message(LOG_LEVEL_ERROR,"Buffer too small (%zu bytes) for conmgt_get_connection_stats.", buffer_size); 
    return -1;
}

/**
 * @brief Gets the current number of active client connections. Thread-safe.
 * @return The number of active connections.
 */
 int conmgt_get_active_connections() {
    int count;
    pthread_mutex_lock(&conmgt_mutex);
    count = num_clients;
    pthread_mutex_unlock(&conmgt_mutex);
    return count;
}