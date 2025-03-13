/*
 * File: chat.c
 * Author: Chau Bui
 * Description: This is the main file for the chat application. It initializes the
 *              program, handles user input, and manages the overall flow of the application.
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <signal.h>       /* For signal handling */
 #include <unistd.h>       /* For close() and sleep() */
 #include <sys/socket.h>   /* For socket operations */
 #include <netinet/in.h>   /* For sockaddr_in */
 #include <pthread.h>      /* For threading */
 #include "utils.h"
 #include "connection_manager.h"
 #include "server.h"
 #include "client.h"
 
 /* Global variables */
 volatile sig_atomic_t running = 1; /* Flag to control program execution */
 int listen_sock;                   /* Listening socket for incoming connections */
 int log_pipe[2];                   /* Pipe for logging output */
 char* myip;                        /* Local IP address */
 int myport;                        /* Listening port number */
 
 /* Signal handler for graceful shutdown */
 /* Sets running to 0 on SIGINT (Ctrl+C) */
 void signal_handler(int sig) {
     running = 0;
 }
 
 /* Thread function to handle logging */
 /* Reads from log_pipe and writes to stdout */
 void* logging_thread(void* arg) {
     char buffer[1024];
     while (true) {
         ssize_t n = read(log_pipe[0], buffer, sizeof(buffer));
         if (n <= 0) break; /* Exit on pipe close or error */
         write(STDOUT_FILENO, buffer, n); /* Write to console */
     }
     return NULL;
 }
 
 /* Function to print the help message */
 /* Displays available commands and their descriptions */
 void print_help() {
     dprintf(log_pipe[1], "\nAvailable commands:\n"
            "help - Display this help message\n"
            "myip - Display the IP address of this process\n"
            "myport - Display the port this process is listening on\n"
            "connect <destination> <port> - Connect to another peer\n"
            "list - List all connections\n"
            "terminate <connection id> - Terminate a connection\n"
            "send <connection id> <message> - Send a message (max 100 chars)\n"
            "exit - Exit the program\n");
 }
 
 /* Main function */
 /* Entry point of the chat application */
 int main(int argc, char* argv[]) {
     /* Validate command-line arguments */
     if (argc != 2) {
         fprintf(stderr, "Usage: ./chat <port>\n");
         return 1;
     }
 
     myport = atoi(argv[1]); /* Parse port number */
     myip = get_local_ip();  /* Get local IP address */
     init_connections();     /* Initialize connection manager */
 
     /* Create pipe for logging */
     if (pipe(log_pipe) == -1) {
         perror("Failed to create pipe");
         return 1;
     }
 
     /* Spawn logging thread */
     pthread_t log_thread;
     pthread_create(&log_thread, NULL, logging_thread, NULL);
     pthread_detach(log_thread);
 
     signal(SIGINT, signal_handler); /* Set up signal handler */
 
     /* Create listening socket */
     listen_sock = socket(AF_INET, SOCK_STREAM, 0);
     if (listen_sock < 0) {
         dprintf(log_pipe[1], "Failed to create socket\n");
         return 1;
     }
 
     /* Set up server address and bind socket */
     struct sockaddr_in server_addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(myport) };
     if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0 || listen(listen_sock, 5) < 0) {
         dprintf(log_pipe[1], "Failed to bind or listen on socket\n");
         close(listen_sock);
         return 1;
     }
 
     /* Spawn server thread to handle incoming connections */
     pthread_t server_thread_id;
     pthread_create(&server_thread_id, NULL, server_thread, NULL);
     pthread_detach(server_thread_id);
 
     dprintf(log_pipe[1], "Chat started on %s:%d\n", myip, myport);
 
     /* Main command loop */
     while (running) {
         dprintf(log_pipe[1], "> "); /* Prompt user for input */
         char line[256];
         if (!fgets(line, sizeof(line), stdin)) break; /* Read command */
         line[strcspn(line, "\n")] = 0; /* Remove newline */
 
         int num_tokens;
         char** tokens = split_command(line, &num_tokens);
         if (num_tokens == 0) {
             free_tokens(tokens, num_tokens);
             continue; /* Skip empty input */
         }
 
         /* Process commands */
         if (!strcmp(tokens[0], "help")) print_help();
         else if (!strcmp(tokens[0], "myip")) dprintf(log_pipe[1], "%s\n", myip);
         else if (!strcmp(tokens[0], "myport")) dprintf(log_pipe[1], "%d\n", myport);
         else if (!strcmp(tokens[0], "connect") && num_tokens == 3) 
             connect_to_peer(tokens[1], atoi(tokens[2]));
         else if (!strcmp(tokens[0], "list")) list_connections();
         else if (!strcmp(tokens[0], "terminate") && num_tokens == 2) 
             remove_connection(atoi(tokens[1]));
         else if (!strcmp(tokens[0], "send") && num_tokens >= 3) {
             int id = atoi(tokens[1]);
             char message[101] = {0}; /* Buffer for message (max 100 chars + null) */
             /* Construct message from tokens */
             for (int i = 2; i < num_tokens; i++) {
                 strncat(message, tokens[i], 100 - strlen(message));
                 if (i < num_tokens - 1) strncat(message, " ", 100 - strlen(message));
             }
             if (strlen(message) > 100) 
                 dprintf(log_pipe[1], "Message too long\n");
             else 
                 send_message(id, message);
         }
         else if (!strcmp(tokens[0], "exit")) running = 0;
         else dprintf(log_pipe[1], "Unknown command\n");
 
         free_tokens(tokens, num_tokens); /* Clean up tokens */
     }
 
     /* Cleanup on exit */
     close(listen_sock); /* Close listening socket */
     pthread_mutex_lock(&connections_mutex);
     for (int i = 0; i < MAX_CONNECTIONS; i++) {
         if (connections[i].sock != -1) {
             close(connections[i].sock); /* Close all active connections */
             connections[i].sock = -1;
         }
     }
     pthread_mutex_unlock(&connections_mutex);
     close(log_pipe[1]); /* Close write end of log pipe */
     sleep(1);           /* Allow threads to exit gracefully */
     dprintf(STDERR_FILENO, "Program exited.\n");
     free(myip);         /* Free allocated IP string */
     return 0;
 }