#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Function to be executed when the child receives SIGUSR1 */
void sigusr1_Handler(int signo) {
    /* Print a message to indicate that the signal was received */
    printf("Child process received SIGUSR1: %d!\n", signo);
}

int main() {
    pid_t pid; /* Variable to store the process ID */

    /* Create a child process using fork() */
    pid = fork();

    if (pid == 0) { /* Child process */
        /* Set up the signal handler for SIGUSR1 */
        if (signal(SIGUSR1, sigusr1_Handler) == SIG_ERR) {
            perror("signal"); /* Print error message if signal setup fails */
            exit(1); /* Exit with error code */
        }

        printf("Child process (PID: %d) is waiting for SIGUSR1...\n", getpid());

        /* Keep the child process running indefinitely to receive signals */
        while (1) {
            pause(); /* Wait for a signal to arrive */
        }
    } else if (pid > 0) { /* Parent process */
        printf("Parent process (PID: %d) created child process (PID: %d)\n", getpid(), pid);

        /* Wait for a short period (e.g., 2 seconds) before sending the signal */
        sleep(2);

        /* Send SIGUSR1 to the child process */
        printf("Parent process sending SIGUSR1 to child process...\n");
        kill(pid, SIGUSR1);

        /* Wait for the child process to finish */
        int status; 
        waitpid(pid, &status, 0);  
        printf("Child process finished.\n"); 
    } else { /* Fork failed */
        perror("fork"); /* Print error message if fork fails */
        return 1; /* Return with error code */
    }

    return 0; /* Return 0 to indicate successful execution */
}