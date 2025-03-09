#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFFER_SIZE 256

int main() {
    int pipefd[2]; /* File descriptors for the pipe */
    pid_t child_pid;
    char message[] = "Hello from the parent process!"; /* Message to be sent */
    char buffer[BUFFER_SIZE];  /* Buffer to store read data */

    /* 1. Create the pipe */
    if (pipe(pipefd) == -1) {
        perror("pipe"); /* Error handling if pipe creation fails */
        exit(EXIT_FAILURE);
    }

    /* 2. Fork a child process */
    child_pid = fork();

    if (child_pid == -1) {
        perror("fork"); /* Error handling if fork fails */
        exit(EXIT_FAILURE);
    }

    if (child_pid == 0) { /* Child process */
        /* Close the write end of the pipe in the child (child only reads) */
        close(pipefd[1]);

        /* 3. Read from the pipe */
        ssize_t bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1);
        if (bytes_read == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        buffer[bytes_read] = '\0'; /* Null-terminate the string */

        /* 4. Print the message */
        printf("Child process received: %s\n", buffer);

        /* Close the read end of the pipe in the child */
        close(pipefd[0]);
        exit(EXIT_SUCCESS); /* Child process exits */

    } else { /* Parent process */
        /* Close the read end of the pipe in the parent (parent only writes) */
        close(pipefd[0]);

        /* 5. Write to the pipe */
        printf("Parent process sending: %s\n", message);
        ssize_t bytes_written = write(pipefd[1], message, strlen(message));
        if (bytes_written == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }

        /* Close the write end of the pipe in the parent */
        close(pipefd[1]);

        /* Wait for the child process to finish (good practice) */
        wait(NULL);
        printf("Parent process finished.\n");
    }

    return 0;
}