#include <stdio.h>      
#include <stdlib.h>     
#include <string.h>    
#include <unistd.h>
#include <fcntl.h> 
#include <sys/stat.h>
#include <mqueue.h> 
#include <sys/wait.h>

/* Define the name of the message queue. Must start with a slash. */
#define QUEUE_NAME    "/my_test_queue"
/* Define the maximum size of a message */
#define MAX_MSG_SIZE  256
/* Define the buffer size for receiving messages */
#define MSG_BUFFER_SIZE (MAX_MSG_SIZE + 10)
/* Define the message priority */
#define MSG_PRIORITY  0

int main() {
    mqd_t mq;                /* Message queue descriptor */
    struct mq_attr attr;     /* Message queue attributes */
    pid_t pid;               /* Process ID for fork */
    char buffer[MSG_BUFFER_SIZE]; /* Buffer for sending/receiving messages */
    ssize_t bytes_read;      /* Number of bytes received */
    int status;              /* Status for waitpid */

    /* Initialize message queue attributes */
    /* These attributes are used when creating the queue */
    attr.mq_flags = 0;              /* Flags (blocking mode) */
    attr.mq_maxmsg = 10;            /* Maximum number of messages in queue */
    attr.mq_msgsize = MAX_MSG_SIZE; /* Maximum message size */
    attr.mq_curmsgs = 0;            /* Number of messages currently in queue (ignored for mq_open) */

    /* Create the message queue */
    /* mq_open creates a new queue or opens an existing one.
       - QUEUE_NAME: The unique identifier for the queue.
       - O_CREAT | O_RDWR: Flags. O_CREAT creates if it doesn't exist. O_RDWR opens for reading and writing.
       - 0666: Permissions for the new queue (similar to file permissions). Read/write for owner, group, others.
       - &attr: Pointer to the attributes structure defined above. */
    mq = mq_open(QUEUE_NAME, O_CREAT | O_RDWR, 0666, &attr);
    if (mq == (mqd_t)-1) {
        /* mq_open returns (mqd_t)-1 on error */
        perror("- mq_open failed");
        exit(EXIT_FAILURE);
    }

    /* Create a child process */
    pid = fork();

    if (pid < 0) {
        /* fork returns -1 on error */
        perror("- fork failed !");
        /* Clean up the message queue before exiting */
        mq_close(mq);
        mq_unlink(QUEUE_NAME);
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        /* ----- Child Process ----- */
        printf("- Child process started (PID: %d) - Waiting for message... \n", getpid());

        /* Receive the message from the queue */
        /* mq_receive waits (blocks) until a message is available.
           - mq: The message queue descriptor.
           - buffer: The buffer to store the received message.
           - MSG_BUFFER_SIZE: The size of the buffer. Must be >= mq_msgsize.
           - NULL: Pointer to store message priority (we ignore it here). */
        bytes_read = mq_receive(mq, buffer, MSG_BUFFER_SIZE, NULL);
        if (bytes_read == -1) {
            /* mq_receive returns -1 on error */
            perror("- mq_receive failed in child ");
             /* Child doesn't unlink, only closes its descriptor */
            mq_close(mq);
            exit(EXIT_FAILURE);
        }

        /* Null-terminate the received data to treat it as a string */
        /* bytes_read contains the actual length of the message received */
        buffer[bytes_read] = '\0';

        /* Print the received message */
        printf("- Child process received message: '%s' \n", buffer);

        /* Close the message queue descriptor in the child process */
        /* Each process that opens the queue gets its own descriptor */
        if (mq_close(mq) == -1) {
            perror("- mq_close failed in child ");
            /* Still exit, maybe with failure, but main task is done */
            exit(EXIT_FAILURE);
        }

        printf("- Child process finished. \n");
        exit(EXIT_SUCCESS); /* Indicate successful execution */

    } else {
        /* ----- Parent Process ----- */
        const char *message = "Hello from Parent Process!";
        printf("- Parent process started (PID: %d) - Sending message... \n", getpid());

        /* Send the message to the queue */
        /* mq_send sends a message to the queue.
           - mq: The message queue descriptor.
           - message: Pointer to the message data.
           - strlen(message): The length of the message (don't send the null terminator explicitly unless needed).
                              mq_receive will get exactly this many bytes.
           - MSG_PRIORITY: The priority of the message. */
        if (mq_send(mq, message, strlen(message), MSG_PRIORITY) == -1) {
            /* mq_send returns -1 on error */
            perror("- mq_send failed in parent ");
            /* Clean up before exiting */
            mq_close(mq);
            mq_unlink(QUEUE_NAME);
            /* Optionally kill the child if it was created? For simplicity, just exit here. */
            exit(EXIT_FAILURE);
        }

        printf("- Parent process sent message: '%s' \n", message);

        /* Wait for the child process to terminate */
        /* This ensures the child finishes processing before the parent cleans up */
        printf("- Parent process waiting for child (PID: %d) to finish... \n", pid);
        if (waitpid(pid, &status, 0) == -1) {
             perror("- waitpid failed ");
             /* Continue with cleanup even if waitpid fails */
        } else {
             printf("- Parent detected child process finished. \n");
        }


        /* Close the message queue descriptor in the parent process */
        if (mq_close(mq) == -1) {
            perror("-  mq_close failed in parent ");
            /* Continue to unlink despite close error */
        }

        /* Remove the message queue from the system */
        /* mq_unlink removes the queue name. If not done, the queue persists
           after the program exits until the system reboots or it's manually removed.
           ( Important to do this after the child is done ) */
        if (mq_unlink(QUEUE_NAME) == -1) {
            perror("- mq_unlink failed in parent ");
            exit(EXIT_FAILURE);
        }

        printf("- Parent process closed and unlinked the queue. Exiting. \n");
        exit(EXIT_SUCCESS); /* Indicate successful execution */
    }

    return 0;
}