#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

// Global variable to count the number of SIGINT signals received
int sigint_count = 0;

// Signal handler function for SIGINT
void sigint_handler(int signum) {
    sigint_count++;
    printf("SIGINT received (%d/3)\n", sigint_count);

    if (sigint_count >= 3) {
        printf("Exiting after 3 SIGINT signals.\n");
        exit(0); // Terminate the program
    }
}

int main() {
    // The signal handler for SIGINT
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        perror("signal"); // Print an error message if signal() fails
        exit(1);
    }

    printf("Press Ctrl+C to send SIGINT (3 times to exit).\n");

    // Infinite loop to keep the program running
    while (1) {
    }

    return 0;
}