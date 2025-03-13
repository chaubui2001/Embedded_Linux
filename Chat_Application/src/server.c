/*
 * File: server.c
 * Author: Chau Bui
 * Description: This file contains the server-side logic for accepting incoming
 *              connections and handling client communication in separate threads.
 */

 #include "server.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>       /* For close() */
 #include <sys/socket.h>   /* For socket operations */
 #include <netinet/in.h>   /* For sockaddr_in */
 #include <arpa/inet.h>    /* For inet_ntop */
 #include <pthread.h>      /* For threading */
 #include <signal.h>       /* For signal handling */
 #include "connection_manager.h"
 #include "utils.h"
 
 /* External declarations of global variables */
 extern int log_pipe[2];
 extern connection_t connections[MAX_CONNECTIONS];
 extern pthread_mutex_t connections_mutex;
 extern volatile sig_atomic_t running;
 extern int listen_sock;
 
 /* Function to handle communication with a connected client */
 /* Runs in a separate thread for each client connection */
 void* handle_client(void* arg) {
     int sock = *(int*)arg; /* Extract socket from argument */
     free(arg);             /* Free allocated memory for argument */
     char buffer[1024];
     while (true) {
         /* Receive data from the client */
         int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
         if (bytes_received <= 0) { /* Connection closed or error */
             break;
         }
 
         buffer[bytes_received] = '\0'; /* Null-terminate the received data */
 
         /* Check for close signal from peer */
         if (strcmp(buffer, "XXXXX") == 0) {
             break; /* Peer signaled connection closure */
         }
         
         /* Retrieve connection details */
         int id = -1;
         char ip[INET_ADDRSTRLEN];
         int port;
         pthread_mutex_lock(&connections_mutex);
         for (int i = 0; i < MAX_CONNECTIONS; i++) {
             if (connections[i].sock == sock) {
                 id = connections[i].id;
                 strcpy(ip, connections[i].ip);
                 port = connections[i].port;
                 break;
             }
         }
         pthread_mutex_unlock(&connections_mutex);
         if (id != -1) {
             /* Print received message with sender details */
             dprintf(log_pipe[1], "\nMessage received from %s\nSender's Port: %d\nMessage: %s\n> ", 
                     ip, port, buffer);
         }
     }
     /* Clean up connection when disconnected */
     pthread_mutex_lock(&connections_mutex);
     for (int i = 0; i < MAX_CONNECTIONS; i++) {
         if (connections[i].sock == sock) {
             close(sock); /* Close the socket */
             connections[i].sock = -1; /* Mark as inactive */
             dprintf(log_pipe[1], "\nConnection %d closed\n> ", connections[i].id);
             break;
         }
     }
     pthread_mutex_unlock(&connections_mutex);
     return NULL;
 }
 
 /* Function to run the server thread */
 /* Listens for incoming connections and spawns client handlers */
 void* server_thread(void* arg) {
     while (running) {
         struct sockaddr_in client_addr;
         socklen_t client_len = sizeof(client_addr);
         /* Accept incoming connection */
         int client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
         if (client_sock < 0) {
             if (!running) break; /* Exit if program is shutting down */
             continue;            /* Skip on error */
         }
         /* Extract client IP and port */
         char ip[INET_ADDRSTRLEN];
         inet_ntop(AF_INET, &client_addr.sin_addr, ip, INET_ADDRSTRLEN);
         int port = ntohs(client_addr.sin_port);
         /* Add new connection */
         int id = add_connection(client_sock, ip, port);
         if (id == -1) {
             dprintf(log_pipe[1], "Maximum connections reached\n");
             close(client_sock);
             continue;
         }
         dprintf(log_pipe[1], "\nNew connection from %s:%d assigned ID %d\n> ", ip, port, id);
         /* Spawn a thread to handle the client */
         int* sock_ptr = malloc(sizeof(int));
         *sock_ptr = client_sock;
         pthread_t thread;
         pthread_create(&thread, NULL, handle_client, sock_ptr);
         pthread_detach(thread); /* Detach to clean up automatically */
     }
     return NULL;
 }