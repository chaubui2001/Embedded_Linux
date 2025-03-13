/*
 * File: connection_manager.c
 * Author: Chau Bui
 * Description: This file implements the connection management system, handling
 *              the addition, removal, and listing of peer connections.
 */

 #include "connection_manager.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>      /* For close() */
 #include "utils.h"
 
 /* External declaration of log pipe for output */
 extern int log_pipe[2];
 
 /* Global variables defined here (declared in header) */
 connection_t connections[MAX_CONNECTIONS];
 int next_id = 1;
 pthread_mutex_t connections_mutex = PTHREAD_MUTEX_INITIALIZER;
 
 /* Function to initialize the connection manager */
 /* Marks all connection slots as inactive by setting sock to -1 */
 void init_connections() {
     for (int i = 0; i < MAX_CONNECTIONS; i++) {
         connections[i].sock = -1;
     }
 }
 
 /* Function to add a new connection */
 /* Assigns an ID and stores connection details; returns ID or -1 if full */
 int add_connection(int sock, const char* ip, int port) {
     pthread_mutex_lock(&connections_mutex); /* Protect shared resource */
     for (int i = 0; i < MAX_CONNECTIONS; i++) {
         if (connections[i].sock == -1) { /* Find an empty slot */
             connections[i].id = next_id++; /* Assign and increment ID */
             connections[i].sock = sock;
             strncpy(connections[i].ip, ip, INET_ADDRSTRLEN);
             connections[i].port = port;
             pthread_mutex_unlock(&connections_mutex);
             return connections[i].id;
         }
     }
     pthread_mutex_unlock(&connections_mutex);
     return -1; /* Return -1 if no slots are available */
 }
 
 /* Function to remove a connection by ID */
 /* Closes the socket and marks it as inactive */
 void remove_connection(int id) {
     pthread_mutex_lock(&connections_mutex);
     for (int i = 0; i < MAX_CONNECTIONS; i++) {
         if (connections[i].id == id && connections[i].sock != -1) {
             /* Send a "CLOSE" signal to the peer before closing */
             send(connections[i].sock, "XXXXX", 5, 0);
             close(connections[i].sock); /* Close the socket */
             connections[i].sock = -1;  /* Mark as inactive */
             dprintf(log_pipe[1], "Connection %d terminated\n", id);
             break;
         }
     }
     pthread_mutex_unlock(&connections_mutex);
 }
 
 /* Function to list all active connections */
 /* Prints a formatted table of connection details */
 void list_connections() {
     pthread_mutex_lock(&connections_mutex);
     int count = 0;
     /* Count active connections */
     for (int i = 0; i < MAX_CONNECTIONS; i++) {
         if (connections[i].sock != -1) {
             count++;
         }
     }
     if (count == 0) {
         dprintf(log_pipe[1], "List is empty\n");
     } else {
         /* Print header */
         dprintf(log_pipe[1], "%-5s %-15s %-10s\n", "ID", "IP address", "Port");
         /* Print each active connection */
         for (int i = 0; i < MAX_CONNECTIONS; i++) {
             if (connections[i].sock != -1) {
                 dprintf(log_pipe[1], "%-5d %-15s %-10d\n", 
                         connections[i].id, 
                         connections[i].ip, 
                         connections[i].port);
             }
         }
     }
     pthread_mutex_unlock(&connections_mutex);
 }
 
 /* Function to check for duplicate connections */
 /* Returns true if a connection to the same IP and port exists */
 bool is_duplicate_connection(const char* ip, int port) {
     pthread_mutex_lock(&connections_mutex);
     for (int i = 0; i < MAX_CONNECTIONS; i++) {
         if (connections[i].sock != -1 && 
             strcmp(connections[i].ip, ip) == 0 && 
             connections[i].port == port) {
             pthread_mutex_unlock(&connections_mutex);
             return true;
         }
     }
     pthread_mutex_unlock(&connections_mutex);
     return false;
 }
 
 /* Function to get the socket for a connection ID */
 /* Returns the socket descriptor or -1 if not found */
 int get_connection_socket(int id) {
     pthread_mutex_lock(&connections_mutex);
     for (int i = 0; i < MAX_CONNECTIONS; i++) {
         if (connections[i].id == id && connections[i].sock != -1) {
             int sock = connections[i].sock;
             pthread_mutex_unlock(&connections_mutex);
             return sock;
         }
     }
     pthread_mutex_unlock(&connections_mutex);
     return -1;
 }