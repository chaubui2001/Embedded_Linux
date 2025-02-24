#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

int main(int argc, char *argv[]) {
    pid_t pid;

    /* Create a child process */
    pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) { /* Child process */
        /* Check if a command-line argument was provided */
        if (argc < 2) {
            fprintf(stderr, "Usage: %s <command>\n", argv[0]);
            fprintf(stderr, "  <command>: 1 for ls -l -h, 2 for date, 3 for ls -l -a\n");
            exit(EXIT_FAILURE);
        }

        char *command = argv[1]; /* Get the command choice from argv[1] */

        printf("Before exec - Child PID: %d\n", getpid());

        if (strcmp(command, "1") == 0) {
            execlp("ls", "ls", "-l", "-h", NULL);
        } else if (strcmp(command, "2") == 0) {
            execlp("date", "date", NULL);
        } else if (strcmp(command, "3") == 0) {
            char *cmd = "ls";
            char *argv[] = { "ls", "-l", "-a", NULL };
            execvp(cmd, argv);
        }else {
            fprintf(stderr, "Invalid command choice: %s\n", command);
            exit(EXIT_FAILURE);
        }

        perror("exec"); /* Only reached if exec fails */
        exit(127);
    } else { /* Parent process */
        int status;
        printf("Parent PID: %d\n", getpid());
        pid_t child_pid = waitpid(pid, &status, 0);
        printf("Child PID returned by waitpid: %d\n", child_pid);

        if (WIFEXITED(status)) {
            printf("Child process exited with status: %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Child process terminated by signal: %d\n", WTERMSIG(status));
        }

        printf("Child process completed.\n");
    }

    return EXIT_SUCCESS;
}