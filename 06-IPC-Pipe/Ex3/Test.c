#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFFER_SIZE 256

int main() {
    int pipefd[2];  /* File descriptors for the pipe */
    pid_t child_pid;
    char message[] = "This is a test string"; /* The string to send */
    char buffer[BUFFER_SIZE];

    /* Create the pipe */
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    /* Fork a child process */
    child_pid = fork();

    if (child_pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (child_pid == 0) { /* Child process */
        /* Close the write end of the pipe (child only reads) */
        close(pipefd[1]);

        /* Read from the pipe */
        ssize_t bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1);
        if (bytes_read == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        buffer[bytes_read] = '\0'; /* Null-terminate the string */

        /* Print the read string */
        printf("Child process: Received string: %s\n", buffer);

        /* Count the characters using strlen() */
        int char_count = strlen(buffer);

        /* Print the character count */
        printf("Child process: Number of characters received: %d\n", char_count);

        /* Close the read end of the pipe */
        close(pipefd[0]);
        exit(EXIT_SUCCESS);

    } else { /* Parent process */
        /* Close the read end of the pipe (parent only writes) */
        close(pipefd[0]);

        /* Write the string to the pipe */
        ssize_t bytes_written = write(pipefd[1], message, strlen(message));
         if (bytes_written == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }

        /* Print the sent string */
        printf("Parent process: Sent string: %s\n", message);

        /* Close the write end of the pipe */
        close(pipefd[1]);

        /* Wait for the child process to finish */
        wait(NULL);
        printf("Parent process finished.\n");
    }

    return 0;
}