/*
 * File: server.h
 * Author: Chau Bui
 * Description: This header file declares the server-side components, including
 *              the listening socket, log pipe, and the server thread function.
 */

 #ifndef SERVER_H
 #define SERVER_H
 #include <pthread.h>
 
 /* External declaration of the listening socket for incoming connections */
 extern int listen_sock;
 
 /* External declaration of the log pipe for output redirection */
 extern int log_pipe[2];
 
 /* Function to run the server thread, handling incoming connections */
 void* server_thread(void* arg);
 
 #endif