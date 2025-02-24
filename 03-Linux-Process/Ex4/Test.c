#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

int main(int argc, char *argv[]) {
    pid_t pid;
    int status;
    int exit_code;

    /* Check if an exit code is provided as a command-line argument */
    if (argc > 1) {
        /* Convert the command-line argument to an integer */
        exit_code = atoi(argv[1]);  // atoi converts string to integer
    } else {
        /* Default exit code if no argument is provided */
        exit_code = 0;
        printf("No exit code provided. Using default exit code: %d\n", exit_code);
    }

    /* Create a child process */
    pid = fork();

    if (pid == 0) { /* Child process */
        printf("Child process (PID: %d) exiting with code: %d\n", getpid(), exit_code);
        exit(exit_code);
    } else if (pid > 0) { /* Parent process */
        printf("Parent process (PID: %d) created child process (PID: %d)\n", getpid(), pid);

        /* Wait for the child process to terminate */
        if (wait(&status) == -1) {
            perror("wait");
            exit(1);
        }

        /* Check the exit status of the child process */
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            printf("Child process exited normally with status: %d\n", exit_status);
        } else {
            printf("Child process did not exit normally.\n");
        }
    } else { /* Fork failed */
        perror("fork");
        return 1;
    }

    return 0;
}