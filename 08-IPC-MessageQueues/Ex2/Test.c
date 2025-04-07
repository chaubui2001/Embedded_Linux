#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <unistd.h>
#include <fcntl.h> 
#include <sys/stat.h>
#include <mqueue.h> 
#include <sys/wait.h>

/* Define names for the two message queues */
#define QUEUE_PARENT_TO_CHILD "/p_to_c_string_queue" /* Parent sends string */
#define QUEUE_CHILD_TO_PARENT "/c_to_p_string_queue" /* Child sends count */

/* Define test message */
#define TEST_MESSAGE    "This is a test string!"

/* Define message properties */
#define MAX_MSG_SIZE      256                      /* Max size for string or count */
#define MSG_BUFFER_SIZE   (MAX_MSG_SIZE + 10)    /* Buffer size */
#define MSG_PRIORITY      0                        /* Default priority */

int main() {
    mqd_t mq_p2c;            /* MSG Queue Descriptor: Parent -> Child */
    mqd_t mq_c2p;            /* MSG Queue Descriptor: Child -> Parent */
    struct mq_attr attr;     /* Message queue attributes */
    pid_t pid;               /* Process ID for fork */
    char buffer[MSG_BUFFER_SIZE]; /* Generic buffer for messages */
    ssize_t bytes_received;  /* Bytes received from mq_receive */
    int status;              /* Status for waitpid */

    /* Initialize message queue attributes */
    attr.mq_flags = 0;              /* Flags (blocking) */
    attr.mq_maxmsg = 10;            /* Max messages */
    attr.mq_msgsize = MAX_MSG_SIZE; /* Max message size */
    attr.mq_curmsgs = 0;            /* Current messages */

    /* --- Create/Open both message queues --- */

    /* Create the Parent -> Child queue */
    mq_p2c = mq_open(QUEUE_PARENT_TO_CHILD, O_CREAT | O_RDWR, 0666, &attr);
    if (mq_p2c == (mqd_t)-1) {
        perror("mq_open (p2c) failed"); /* Error messages still use perror */
        exit(EXIT_FAILURE);
    }

    /* Create the Child -> Parent queue */
    mq_c2p = mq_open(QUEUE_CHILD_TO_PARENT, O_CREAT | O_RDWR, 0666, &attr);
    if (mq_c2p == (mqd_t)-1) {
        perror("mq_open (c2p) failed");
        /* Cleanup the first queue if the second fails */
        mq_close(mq_p2c);
        mq_unlink(QUEUE_PARENT_TO_CHILD);
        exit(EXIT_FAILURE);
    }

    /* Fork a child process */
    pid = fork();

    if (pid < 0) {
        /* Fork failed */
        perror("fork failed");
        /* Cleanup both queues before exiting */
        mq_close(mq_p2c);
        mq_close(mq_c2p);
        mq_unlink(QUEUE_PARENT_TO_CHILD);
        mq_unlink(QUEUE_CHILD_TO_PARENT);
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        /* ----- Child Process ----- */
        int char_count = 0; /* Variable to store character count */

        printf("- Child process (PID: %d) started. Waiting for string...\n", getpid());

        /* Receive the string message from the parent */
        bytes_received = mq_receive(mq_p2c, buffer, MSG_BUFFER_SIZE, NULL);
        if (bytes_received == -1) {
            perror("mq_receive (p2c) failed in child");
            /* Close descriptors before exiting */
            mq_close(mq_p2c);
            mq_close(mq_c2p);
            exit(EXIT_FAILURE);
        }

        /* Null-terminate the received string */
        buffer[bytes_received] = '\0';
        printf("- Child received string: '%s'\n", buffer);

        /* Count the characters in the received string */
        char_count = strlen(buffer);
        printf("- Child calculated count: %d\n", char_count);

        /* Prepare the count message (convert integer to string) */
        int chars_written = snprintf(buffer, MSG_BUFFER_SIZE, "%d", char_count);
        if (chars_written < 0 || chars_written >= MSG_BUFFER_SIZE) {
             fprintf(stderr, "- Error: snprintf failed or buffer too small in child\n");
             mq_close(mq_p2c);
             mq_close(mq_c2p);
             exit(EXIT_FAILURE);
        }


        /* Send the character count (as a string) back to the parent */
        if (mq_send(mq_c2p, buffer, strlen(buffer), MSG_PRIORITY) == -1) {
            perror("mq_send (c2p) failed in child");
            /* Close descriptors before exiting */
            mq_close(mq_p2c);
            mq_close(mq_c2p);
            exit(EXIT_FAILURE);
        }
        printf("- Child sent count back to parent.\n");

        /* Close both message queue descriptors in the child */
        if (mq_close(mq_p2c) == -1) {
            perror("mq_close(p2c) failed in child");
            /* Continue closing the other one */
        }
         if (mq_close(mq_c2p) == -1) {
            perror("mq_close(c2p) failed in child");
        }

        printf("- Child process finished.\n");
        exit(EXIT_SUCCESS);

    } else {
        /* ----- Parent Process ----- */
        const char *message_to_send = TEST_MESSAGE;
        int received_count = -1; /* Variable to store the received count */

        printf("- Parent process (PID: %d) started. Sending string...\n", getpid());

        /* Send the string message to the child */
        if (mq_send(mq_p2c, message_to_send, strlen(message_to_send), MSG_PRIORITY) == -1) {
            perror("mq_send (p2c) failed in parent");
            /* Cleanup both queues */
            mq_close(mq_p2c);
            mq_close(mq_c2p);
            mq_unlink(QUEUE_PARENT_TO_CHILD);
            mq_unlink(QUEUE_CHILD_TO_PARENT);
            exit(EXIT_FAILURE);
        }
        printf("- Parent sent string: '%s'\n", message_to_send);

        /* Wait to receive the character count back from the child */
        printf("- Parent waiting for count from child...\n");
        bytes_received = mq_receive(mq_c2p, buffer, MSG_BUFFER_SIZE, NULL);
        if (bytes_received == -1) {
            perror("mq_receive (c2p) failed in parent");
             /* Cleanup both queues */
            mq_close(mq_p2c);
            mq_close(mq_c2p);
            mq_unlink(QUEUE_PARENT_TO_CHILD);
            mq_unlink(QUEUE_CHILD_TO_PARENT);
            exit(EXIT_FAILURE);
        }

        /* Null-terminate the received count string */
        buffer[bytes_received] = '\0';

        /* Convert the received count string back to an integer */
        received_count = atoi(buffer);
        printf("- Parent received count: %d\n", received_count);

        /* Wait for the child process to terminate */
        printf("- Parent waiting for child (PID: %d) to exit...\n", pid);
         if (waitpid(pid, &status, 0) == -1) {
             perror("waitpid failed in parent");
             /* Continue cleanup even if waitpid fails */
         } else {
              printf("- Parent detected child process has exited.\n");
         }

        /* Close both message queue descriptors in the parent */
         if (mq_close(mq_p2c) == -1) {
            perror("mq_close(p2c) failed in parent");
        }
         if (mq_close(mq_c2p) == -1) {
            perror("mq_close(c2p) failed in parent");
        }

        /* Unlink (remove) both message queues from the system */
        printf("- Parent unlinking queues...\n");
        if (mq_unlink(QUEUE_PARENT_TO_CHILD) == -1) {
            perror("mq_unlink (p2c) failed in parent");
             /* Attempt to unlink the other queue anyway */
        }
         if (mq_unlink(QUEUE_CHILD_TO_PARENT) == -1) {
            perror("mq_unlink (c2p) failed in parent");
        }

        printf("- Parent process finished.\n");
        exit(EXIT_SUCCESS);
    }

    return 0;
}