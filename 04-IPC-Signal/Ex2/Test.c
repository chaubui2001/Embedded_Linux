#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

// Global variable to store the timer count
int timer_count = 0;

// Signal handler function for SIGALRM
void alarm_handler(int signum) {
    timer_count++;
    printf("Timer: %d seconds\n", timer_count);

    if (timer_count >= 10) {
        printf("Timer finished.\n");
        exit(0); // Exit the program
    }

    // Re-arm the alarm for the next second
    alarm(1);
}

int main() {
    // Register the signal handler for SIGALRM
    if (signal(SIGALRM, alarm_handler) == SIG_ERR) {
        perror("signal");
        exit(1);
    }

    printf("Timer starting...\n");

    // Set the initial alarm for 1 second
    alarm(1);

    // Keep the program running
    while (1) {

    }

    return 0;
}