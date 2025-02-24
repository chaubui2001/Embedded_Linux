#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

void sigtstp_handler(int signum) {
    printf("SIGTSTP ignored\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <0|1>\n", argv[0]);
        fprintf(stderr, "  0: Use signal()\n");
        fprintf(stderr, "  1: Use sigaction()\n");
        exit(1);
    }

    int use_sigaction;
    if (strcmp(argv[1], "0") == 0) {
        use_sigaction = 0; // Use signal()
    } else if (strcmp(argv[1], "1") == 0) {
        use_sigaction = 1; // Use sigaction()
    } else {
        fprintf(stderr, "Invalid argument.  Must be 0 or 1.\n");
        exit(1);
    }

    if (use_sigaction) {
        // Use sigaction()
        struct sigaction sa;
        sa.sa_handler = sigtstp_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;

        if (sigaction(SIGTSTP, &sa, NULL) == -1) {
            perror("sigaction");
            exit(1);
        }
    } else {
        // Use signal()
        if (signal(SIGTSTP, sigtstp_handler) == SIG_ERR) {
            perror("signal");
            exit(1);
        }
    }

    printf("Press Ctrl+Z to test (SIGTSTP will be ignored).\n");
    printf("Using %s\n", use_sigaction ? "sigaction()" : "signal()");

    // Keep the program running
    while (1) {
        printf("Running...\n");
        sleep(1); // to view that the program still running
    }

    return 0;
}