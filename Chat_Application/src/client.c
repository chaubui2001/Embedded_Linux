/*
 * File: client.c
 * Author: Chau Bui
 * Description: This file contains the client-side logic for connecting to peers
 *              and sending messages through established connections.
 */

 #include "client.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>       /* For close() */
 #include <sys/socket.h>   /* For socket operations */
 #include <netinet/in.h>   /* For sockaddr_in */
 #include <arpa/inet.h>    /* For inet_pton */
 #include <pthread.h>      /* For threading */
 #include "connection_manager.h"
 #include "utils.h"
 
 /* External declarations of global variables and functions */
 extern int log_pipe[2];
 extern void* handle_client(void* arg);
 extern char* myip;
 extern int myport;
 
 /* Function to connect to a peer */
 /* Establishes a TCP connection and spawns a handler thread */
 bool connect_to_peer(const char* ip, int port) {
     if (!is_valid_ip(ip)) {
         dprintf(log_pipe[1], "Invalid IP address\n");
         return false;
     }
     /* Prevent self-connection */
     if (strcmp(ip, myip) == 0 && port == myport) {
         dprintf(log_pipe[1], "Cannot connect to self\n");
         return false;
     }
     /* Prevent duplicate connections */
     if (is_duplicate_connection(ip, port)) {
         dprintf(log_pipe[1], "Already connected to this peer\n");
         return false;
     }
     /* Create a TCP socket */
     int sock = socket(AF_INET, SOCK_STREAM, 0);
     if (sock < 0) {
         dprintf(log_pipe[1], "Failed to create socket\n");
         return false;
     }
     /* Set up server address structure */
     struct sockaddr_in server_addr = { .sin_family = AF_INET, .sin_port = htons(port) };
     if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
         dprintf(log_pipe[1], "Invalid IP address\n");
         close(sock);
         return false;
     }
     /* Attempt to connect to the peer */
     if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
         dprintf(log_pipe[1], "Connection to %s:%d failed\n", ip, port);
         close(sock);
         return false;
     }
     /* Add the connection to the list */
     int id = add_connection(sock, ip, port);
     if (id == -1) {
         dprintf(log_pipe[1], "Maximum connections reached\n");
         close(sock);
         return false;
     }
     dprintf(log_pipe[1], "Connected to %s:%d as connection ID %d\n", ip, port, id);
     /* Spawn a thread to handle incoming messages */
     int* sock_ptr = malloc(sizeof(int));
     *sock_ptr = sock;
     pthread_t thread;
     pthread_create(&thread, NULL, handle_client, sock_ptr);
     pthread_detach(thread);
     return true;
 }
 
 /* Function to send a message to a connection */
 /* Sends the message via the specified connection ID */
 void send_message(int id, const char* message) {
     int sock = get_connection_socket(id);
     if (sock == -1) {
         dprintf(log_pipe[1], "Connection %d not found\n", id);
         return;
     }
     /* Send the message and check for errors */
     if (send(sock, message, strlen(message), 0) < 0) {
         dprintf(log_pipe[1], "Failed to send message to %d\n", id);
     } else {
         dprintf(log_pipe[1], "Message sent to %d\n", id);
     }
 }