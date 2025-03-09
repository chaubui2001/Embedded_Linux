#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFFER_SIZE 256

int main() {
    int pipe_parent_child1[2]; /* Pipe for parent -> child1 communication */
    int pipe_child1_child2[2]; /* Pipe for child1 -> child2 communication */
    pid_t child1_pid, child2_pid;
    char initial_message[] = "Hello from parent!"; /* Initial message from parent */
    char buffer[BUFFER_SIZE];

    /* Create the first pipe (parent -> child1) */
    if (pipe(pipe_parent_child1) == -1) {
        perror("pipe_parent_child1");
        exit(EXIT_FAILURE);
    }

    /* Create the second pipe (child1 -> child2) */
    if (pipe(pipe_child1_child2) == -1) {
        perror("pipe_child1_child2");
        exit(EXIT_FAILURE);
    }

    /* Fork the first child (child1) */
    child1_pid = fork();

    if (child1_pid == -1) {
        perror("fork child1");
        exit(EXIT_FAILURE);
    }

    if (child1_pid == 0) { /* Child1 process */
        /* Close unused pipe ends */
        close(pipe_parent_child1[1]); /* Child1 doesn't write to parent */
        close(pipe_child1_child2[0]); /* Child1 doesn't read from child2 */

        /* Read from parent */
        ssize_t bytes_read = read(pipe_parent_child1[0], buffer, sizeof(buffer) - 1);
        if (bytes_read == -1) {
            perror("read in child1");
            exit(EXIT_FAILURE);
        }
        buffer[bytes_read] = '\0'; /* Null-terminate */

        /* Print the received message */
        printf("Child1 received: %s\n", buffer);

        /* Modify the message */
        strcat(buffer, " (Appended by child1)");

        /* Write to child2 */
        ssize_t bytes_written = write(pipe_child1_child2[1], buffer, strlen(buffer));
        if (bytes_written == -1) {
            perror("write in child1");
            exit(EXIT_FAILURE);
        }

        /* Close remaining pipe ends */
        close(pipe_parent_child1[0]);
        close(pipe_child1_child2[1]);
        exit(EXIT_SUCCESS);
    }

    /* Fork the second child (child2) */
    child2_pid = fork();

    if (child2_pid == -1) {
        perror("fork child2");
        exit(EXIT_FAILURE);
    }

    if (child2_pid == 0) { /* Child2 process */
        /* Close unused pipe ends */
        close(pipe_parent_child1[0]);
        close(pipe_parent_child1[1]);
        close(pipe_child1_child2[1]);

        /* Read from child1 */
        ssize_t bytes_read = read(pipe_child1_child2[0], buffer, sizeof(buffer) - 1);
        if (bytes_read == -1) {
            perror("read in child2");
            exit(EXIT_FAILURE);
        }
        buffer[bytes_read] = '\0'; /* Null-terminate */

        /* Print the received message */
        printf("Child2 received: %s\n", buffer);

        /* Close the remaining pipe end */
        close(pipe_child1_child2[0]);
        exit(EXIT_SUCCESS);

    } else { /* Parent process */

        /* Close unused pipe ends */
        close(pipe_parent_child1[0]);
        close(pipe_child1_child2[0]);
        close(pipe_child1_child2[1]);

        /* Write the initial message to child1 */
        printf("Parent sending: %s\n", initial_message);
        ssize_t bytes_written = write(pipe_parent_child1[1], initial_message, strlen(initial_message));
        if (bytes_written == -1)
             {
            perror("write in parent");
            exit(EXIT_FAILURE);
        }

        /* Close the write end */
        close(pipe_parent_child1[1]);

        /* Wait for both child processes to finish */
        wait(NULL);
        wait(NULL);
        printf("Parent process finished.\n");
    }

    return 0;
}