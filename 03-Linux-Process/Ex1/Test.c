#include <stdio.h>   
#include <stdlib.h>  
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {
  pid_t pid;

  /* Create a child process */
  pid = fork();

  if (pid < 0) {
    /* Error handling: fork() failed */
    fprintf(stderr, "Fork failed\n");
    return 1; /* Return an error code */

  } else if (pid == 0) {
    /* Child process */
    printf("Child process:\n");
    printf("- Child PID: %d\n", getpid());
    printf("- Parent PID: %d\n", getppid());
    exit(0);
    
  } else {
    /* Parent process */
    printf("Parent process:\n");
    printf("- Child PID: %d\n", pid);
    wait(NULL);
    printf("Child process completed.\n");
  }

  return 0;
}