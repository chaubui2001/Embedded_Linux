#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h> // For strcmp

int main(int argc, char *argv[]) {
    // Check for correct usage
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <0|1>\n", argv[0]);
        fprintf(stderr, "  0: Create a zombie process\n");
        fprintf(stderr, "  1: Create an orphan process\n");
        exit(1);
    }

    int choice;
    if (strcmp(argv[1], "0") == 0) {
        choice = 0; // Zombie case
    } else if (strcmp(argv[1], "1") == 0) {
        choice = 1; // Orphan case
    } else {
        fprintf(stderr, "Invalid argument.  Must be 0 or 1.\n");
        exit(1);
    }


    pid_t pid = fork();

    if (pid < 0) {
        // Fork failed
        perror("fork failed");
        exit(1);
    }

    if (choice == 0) { // Zombie case
        if (pid == 0) {
            // Child process
            printf("Child process (PID: %d) is exiting...\n", getpid());
            exit(0); // Child process terminates
        } else {
            // Parent process
            printf("Parent process (PID: %d) created child (PID: %d)\n", getpid(), pid);
            sleep(10); // Parent process sleeps, does not call wait() immediately
            printf("Parent process exiting...\n");
            // This creates the zombie.
        }
    } else { // Orphan case (choice == 1)
        if (pid == 0) {
            // Child process
            printf("Child process (PID: %d) starting...\n", getpid());
            sleep(10); // Child process sleeps for a long time
            printf("Child process (PID: %d) exiting...\n", getpid());
        } else {
            // Parent process
            printf("Parent process (PID: %d) created child (PID: %d)\n", getpid(), pid);
            printf("Parent process exiting...\n");
            exit(0); // Parent process terminates immediately
        }
    }

    return 0;
}