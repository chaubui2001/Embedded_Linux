#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

// Global variable to count the number of signals sent
int signal_count = 0;
pid_t child_pid; // Store the child process's PID

// Signal handler for the child process
void child_signal_handler(int signum) {
    static int ReceivedSignal_count = 0;
    printf("Received signal from parent (%d/5)\n", ++ReceivedSignal_count);
    if(ReceivedSignal_count == 5){
      printf("Child process exiting.\n");
      exit(0);
    }
}

// Signal handler for SIGALRM in the parent process
void parent_alarm_handler(int signum) {
    if (signal_count < 5) {
        if (kill(child_pid, SIGUSR1) == -1) { // Send SIGUSR1 to the child
            perror("kill");
            exit(1);
        }
        signal_count++;
        printf("Parent sent signal to child (%d/5)\n", signal_count);
        alarm(2); // Set the alarm again for 2 seconds later
    } else {
        wait(NULL); // Wait for the child to finish
        printf("Parent process exiting.\n");
        exit(0); // Exit the parent process after sending 5 signals
    }
}

int main() {
    child_pid = fork();

    if (child_pid < 0) {
        perror("fork failed");
        exit(1);
    } else if (child_pid == 0) {
        // Child process

        // Register the signal handler for SIGUSR1
        if (signal(SIGUSR1, child_signal_handler) == SIG_ERR) {
            perror("signal");
            exit(1);
        }

        // Keep the child process running
        while (1) {

        }
    } else {
        // Parent process

        // Register the signal handler for SIGALRM
        if (signal(SIGALRM, parent_alarm_handler) == SIG_ERR) {
            perror("signal");
            exit(1);
        }

        // Set the initial alarm for 2 seconds
        alarm(2);

        // Keep the parent process running
        while (1) {

        }
    }

    return 0;
}