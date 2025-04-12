#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h> // For UNIX domain sockets
#include <sys/stat.h> // For socket file permissions if needed
#include <errno.h>
#include <pthread.h> // For thread safety if calling managers directly

/* Include project-specific headers */
#include "cmdif.h"
#include "config.h" // For CMD_SOCKET_PATH
#include "common.h" // For error codes maybe
#include "conmgt.h" // To get connection info
#include "sysmon.h" // To get system stats

/* Define buffer sizes for command and response handling */
#define CMD_BUFFER_SIZE 128
#define RESPONSE_BUFFER_SIZE 4096 // Adjust as needed for stats output

/* Global flag to signal shutdown */
static volatile int cmdif_terminate_flag = 0;

/* Listening socket descriptor */
static int listen_sd = -1;

/* Function to stop the command interface */
void cmdif_stop(void) {
    /* Set the flag to signal the loop to exit */
    cmdif_terminate_flag = 1;

    /* Forcefully close the listening socket to unblock accept() */
    if (listen_sd != -1) {
        /* Shutdown the socket for both read and write operations */
        shutdown(listen_sd, SHUT_RDWR);
        close(listen_sd);
        listen_sd = -1;

        /* Remove the socket file to clean up */
        unlink(CMD_SOCKET_PATH);
    }

    /* Log the stop request */
    printf("INFO: Command interface stop requested.\n");
}

/* Function to run the command interface in a separate thread */
void *cmdif_run(void *arg) {
    int client_sd;
    struct sockaddr_un server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char command_buffer[CMD_BUFFER_SIZE];
    char response_buffer[RESPONSE_BUFFER_SIZE];

    /* Parse arguments for socket path */
    cmdif_args_t *args = (cmdif_args_t *)arg;
    const char *socket_path = (args && args->socket_path) ? args->socket_path : CMD_SOCKET_PATH;

    /* Create a UNIX domain socket */
    listen_sd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_sd < 0) {
        perror("ERROR: cmdif socket() failed");
        return NULL;
    }

    /* Prepare the server address structure */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, socket_path, sizeof(server_addr.sun_path) - 1);

    /* Remove any existing socket file before binding */
    unlink(socket_path);

    /* Bind the socket to the specified path */
    if (bind(listen_sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("ERROR: cmdif bind() failed");
        close(listen_sd);
        listen_sd = -1;
        unlink(socket_path); /* Clean up */
        return NULL;
    }

    /* Start listening for incoming connections */
    if (listen(listen_sd, 5) < 0) { /* Backlog of 5 */
        perror("ERROR: cmdif listen() failed");
        close(listen_sd);
        listen_sd = -1;
        unlink(socket_path);
        return NULL;
    }

    /* Log that the command interface is ready */
    printf("INFO: Command interface listening on %s\n", socket_path);

    /* Main loop to accept and process client connections */
    while (!cmdif_terminate_flag) {
        /* Accept a new client connection */
        client_sd = accept(listen_sd, (struct sockaddr *)&client_addr, &client_len);
        if (client_sd < 0) {
            if (cmdif_terminate_flag) {
                /* Log if the accept call was interrupted by shutdown */
                printf("INFO: cmdif accept() interrupted by shutdown.\n");
            } else {
                perror("ERROR: cmdif accept() failed");
            }
            break; /* Exit loop on error or shutdown */
        }

        /* Log that a connection was received */
        printf("INFO: cmdif received connection\n");

        /* Read the command from the client */
        memset(command_buffer, 0, sizeof(command_buffer));
        ssize_t bytes_read = read(client_sd, command_buffer, sizeof(command_buffer) - 1);

        if (bytes_read > 0) {
            /* Remove trailing newline/whitespace from the command */
            command_buffer[strcspn(command_buffer, "\r\n")] = 0;
            printf("DEBUG: Received command: '%s'\n", command_buffer);

            /* Process the received command */
            memset(response_buffer, 0, sizeof(response_buffer));
            if (strcmp(command_buffer, "stats") == 0) {
                /* Retrieve connection stats */
                int count = conmgt_get_connection_stats(response_buffer, sizeof(response_buffer));
                if (count < 0) {
                    snprintf(response_buffer, sizeof(response_buffer), "ERROR: Failed to get stats or buffer too small\n");
                } else if (count == 0) {
                    snprintf(response_buffer, sizeof(response_buffer), "No active connections.\n");
                }

            } else if (strcmp(command_buffer, "status") == 0) {
                /* Retrieve system and connection status */
                int active_conn = conmgt_get_active_connections();
                system_stats_t sys_stats;
                int sysmon_ret = sysmon_get_stats(&sys_stats);

                snprintf(response_buffer, sizeof(response_buffer),
                        "--- System Status ---\n"
                        "Active Connections: %d\n"
                        "CPU Usage: %.2f %%\n"
                        "RAM Usage: %.2f %% (%ld / %ld KB used)\n"
                        "%s",
                        active_conn,
                        sysmon_ret == 0 ? sys_stats.cpu_usage_percent : -1.0,
                        sysmon_ret == 0 ? sys_stats.ram_usage_percent : -1.0,
                        sysmon_ret == 0 ? sys_stats.ram_used_kb : -1L,
                        sysmon_ret == 0 ? sys_stats.ram_total_kb : -1L,
                        sysmon_ret != 0 ? "ERROR: Could not retrieve system stats \n" : ""
                        );

            } else {
                /* Handle unknown commands */
                snprintf(response_buffer, sizeof(response_buffer), "ERROR: Unknown command '%s'. Use 'stats' or 'status'.\n", command_buffer);
            }

            /* Send the response back to the client */
            if (write(client_sd, response_buffer, strlen(response_buffer)) < 0) {
                perror("ERROR: cmdif write() failed");
            }

        } else if (bytes_read == 0) {
            /* Log if the client disconnected without sending a command */
            printf("INFO: cmdif client disconnected without sending command.\n");
        } else {
            /* Log any read errors */
            perror("ERROR: cmdif read() failed");
        }

        /* Close the client connection */
        close(client_sd);
        printf("INFO: cmdif closed connection\n");
    }

    /* Cleanup on shutdown */
    printf("INFO: Command interface thread shutting down.\n");
    if (listen_sd != -1) {
        close(listen_sd);
        listen_sd = -1;
        unlink(socket_path); /* Remove socket file on clean exit */
    }

    return NULL;
}