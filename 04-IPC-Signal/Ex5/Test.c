#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <poll.h>
#include <errno.h>
#include <string.h>

// Use volatile sig_atomic_t for signal flags.
volatile sig_atomic_t sigint_received = 0;
volatile sig_atomic_t sigterm_received = 0;

// Signal handler for SIGINT
void sigint_handler(int signum) {
    sigint_received = 1; // Set the flag to indicate SIGINT was received
}

// Signal handler for SIGTERM
void sigterm_handler(int signum) {
    sigterm_received = 1; // Set the flag to indicate SIGTERM was received
}

int main(int argc, char *argv[]) {
    // Choosing between select() and poll()
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <0|1>\n", argv[0]);
        fprintf(stderr, "  0: Use select()\n");
        fprintf(stderr, "  1: Use poll()\n");
        exit(1);
    }

    int use_poll;
    if (strcmp(argv[1], "0") == 0) {
        use_poll = 0; // Use select()
    } else if (strcmp(argv[1], "1") == 0) {
        use_poll = 1; // Use poll()
    } else {
        fprintf(stderr, "Invalid argument. Must be 0 or 1.\n");
        exit(1);
    }

    // Signal Handler Setup
    struct sigaction sa_int, sa_term;

    // Set up the handler for SIGINT
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);      // Clear the signal mask
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);  // Register the handler

    // Set up the handler for SIGTERM
    sa_term.sa_handler = sigterm_handler;
    sigemptyset(&sa_term.sa_mask);      // Clear the signal mask
    sa_term.sa_flags = 0;
    sigaction(SIGTERM, &sa_term, NULL);  // Register the handler


    char buffer[256];
    printf("Process PID: %d\n", getpid()); // Print the process ID
    printf("Enter input (or press Ctrl+C for SIGINT, or send SIGTERM to exit):\n");
    printf("Using %s\n", use_poll ? "poll()" : "select()");

    while (1) {
        if (use_poll) {
            // --- Using poll() ---

            // Array of pollfd structures
            struct pollfd fds[1];

            // Set up the pollfd structure
            fds[0].fd = STDIN_FILENO;
            fds[0].events = POLLIN; 

            // Call poll() with a 20-second timeout (20000 milliseconds)
            int ready = poll(fds, 1, 20000);

            if (ready == -1) {
                // Error handling
                if (errno == EINTR) {
                    // Check which signal(s) were received using the flags
                    if (sigint_received) {
                        printf("SIGINT received.\n");
                        sigint_received = 0; // Reset the flag
                    }
                    if (sigterm_received) {
                        printf("SIGTERM received. Exiting.\n");
                        exit(0); // Exit on SIGTERM
                    }
                    continue; // Back to the beginning of the loop
                } else {
                    perror("poll"); // Other error
                    exit(1);
                }
            } else if (ready == 0) {
                // Timeout occurred (no input and no signals)
                printf("Timeout. No input received.\n");
                continue;
            }

            // Check if the event was input on stdin
            if (fds[0].revents & POLLIN) {
                // Read input
                fgets(buffer, sizeof(buffer), stdin);
                // Remove newline
                if (strlen(buffer) > 0 && buffer[strlen(buffer) - 1] == '\n') {
                    buffer[strlen(buffer) - 1] = '\0';
                }
                printf("User has entered: %s\n", buffer);
                if (strcmp(buffer, "exit") == 0) {
                  printf("Exiting the program normally\n");
                  break;
                }
            }

        } else {
            // --- Using select() ---

            // Set of file descriptors to monitor
            fd_set readfds;
            // Timeout structure
            struct timeval timeout;
            // Highest file descriptor number + 1
            int nfds = STDIN_FILENO + 1;

            // Clear the set and add standard input
            FD_ZERO(&readfds);
            FD_SET(STDIN_FILENO, &readfds);

            // Set the timeout (20 seconds)
            timeout.tv_sec = 20;
            timeout.tv_usec = 0;

            // Call select()
            int ready = select(nfds, &readfds, NULL, NULL, &timeout);

            if (ready == -1) {
                // Error handling
                if (errno == EINTR) {
                    // Check and handle signals (same as with poll())
                    if (sigint_received) {
                        printf("SIGINT received.\n");
                        sigint_received = 0; // Reset the flag
                    }
                    if (sigterm_received) {
                        printf("SIGTERM received. Exiting.\n");
                        exit(0);
                    }
                    continue; // Back to the beginning of the loop
                } else {
                    perror("select");
                    exit(1);
                }
            } else if (ready == 0) {
                // Timeout occurred
                printf("Timeout. No input received.\n");
                continue;
            }

            // Check if stdin is ready for reading
            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                // Read input
                fgets(buffer, sizeof(buffer), stdin);
                // Remove newline
			        	if (strlen(buffer) > 0 && buffer[strlen(buffer) -1] == '\n'){
				          buffer[strlen(buffer) -1] = '\0';
			        	}
                printf("User has entered: %s\n", buffer);
                if(strcmp(buffer, "exit") == 0) {
				          printf("Exiting the program normally\n");
				          break;
			        }
            }
        }
    }

    return 0;
}