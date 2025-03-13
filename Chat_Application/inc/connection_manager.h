/*
 * File: connection_manager.h
 * Author: Chau Bui
 * Description: This header file defines the connection management system,
 *              including structures and functions to handle multiple connections.
 */

 #ifndef CONNECTION_MANAGER_H
 #define CONNECTION_MANAGER_H
 #include <stdbool.h>
 #include <pthread.h>
 #include <arpa/inet.h>
 
 /* Maximum number of simultaneous connections supported */
 #define MAX_CONNECTIONS 100
 
 /* Structure to store connection details */
 typedef struct {
     int id;        /* Unique identifier for the connection */
     int sock;      /* Socket file descriptor for the connection */
     char ip[INET_ADDRSTRLEN]; /* IP address of the connected peer (IPv4 string) */
     int port;      /* Port number of the connected peer */
 } connection_t;
 
 /* Global array to store all connections */
 extern connection_t connections[MAX_CONNECTIONS];
 /* Global counter for the next connection ID */
 extern int next_id;
 /* Mutex to protect access to the connections array */
 extern pthread_mutex_t connections_mutex;
 
 /* Function to initialize the connection manager */
 /* Sets all connection sockets to -1 (inactive) */
 void init_connections();
 
 /* Function to add a new connection */
 /* Returns the assigned ID or -1 if no slots are available */
 int add_connection(int sock, const char* ip, int port);
 
 /* Function to remove a connection by ID */
 /* Closes the socket and marks it as inactive */
 void remove_connection(int id);
 
 /* Function to list all active connections */
 /* Prints connection details to the log pipe */
 void list_connections();
 
 /* Function to check for duplicate connections */
 /* Returns true if a connection to the IP and port already exists */
 bool is_duplicate_connection(const char* ip, int port);
 
 /* Function to get the socket for a connection ID */
 /* Returns the socket descriptor or -1 if not found */
 int get_connection_socket(int id);
 
 #endif