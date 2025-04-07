#include <stdio.h>
#include <stdlib.h> 
#include <string.h>  
#include <unistd.h>
#include <fcntl.h> 
#include <sys/stat.h>
#include <mqueue.h> 
#include <sys/wait.h>
#include <ctype.h>

/* Define the single message queue name */
#define QUEUE_NAME        "/common_queue"
/* Define message properties */
#define MAX_MSG_SIZE      256
#define MSG_BUFFER_SIZE   (MAX_MSG_SIZE + 10)

/* Define message priorities to control the flow */
#define PRIORITY_PARENT_TO_CHILD1 10 /* Initial message */
#define PRIORITY_CHILD1_TO_CHILD2 20 /* Uppercase message */

/* --- MACRO DEFINITION FOR THE INITIAL MESSAGE --- */
#define MESSAGE_CONTENT "this is a test message !"

int main() {
    mqd_t mq;                /* Single message queue descriptor */
    struct mq_attr attr;     /* Message queue attributes */
    pid_t pid1 = -1, pid2 = -1; /* PIDs for the two child processes */
    char buffer[MSG_BUFFER_SIZE]; /* Message buffer */
    ssize_t bytes_received;  /* Bytes received */
    unsigned int received_priority; /* To store the priority of received msg */
    int status1, status2;    /* Status for waitpid */

    /* Initialize message queue attributes */
    attr.mq_flags = 0;           /* Flags (blocking) */
    attr.mq_maxmsg = 10;         /* Max messages in queue (>=2 needed) */
    attr.mq_msgsize = MAX_MSG_SIZE; /* Max message size */
    attr.mq_curmsgs = 0;         /* Current messages (ignored for mq_open) */

    /* Create the message queue */
    mq = mq_open(QUEUE_NAME, O_CREAT | O_RDWR, 0666, &attr);
    if (mq == (mqd_t)-1) {
        perror("mq_open failed");
        exit(EXIT_FAILURE);
    }
    printf("- Message queue '%s' created.\n", QUEUE_NAME);

    /* --- Fork Child 1 --- */
    pid1 = fork();
    if (pid1 < 0) {
        perror("fork (child 1) failed");
        mq_close(mq);
        mq_unlink(QUEUE_NAME);
        exit(EXIT_FAILURE);
    }

    if (pid1 == 0) {
        /* ----- Child Process 1 ----- */
        mqd_t child1_mq;
        printf("- Child 1 (PID: %d) started.\n", getpid());
        child1_mq = mq_open(QUEUE_NAME, O_RDWR);
        if (child1_mq == (mqd_t)-1) {
            perror("Child 1 mq_open failed"); exit(EXIT_FAILURE); }

        printf("- Child 1 waiting for message from parent...\n");
        bytes_received = mq_receive(child1_mq, buffer, MSG_BUFFER_SIZE, &received_priority);
        if (bytes_received == -1) {
            perror("Child 1 mq_receive failed"); mq_close(child1_mq); exit(EXIT_FAILURE); }

        buffer[bytes_received] = '\0';
        printf("- Child 1 received message (Priority: %u): '%s'\n", received_priority, buffer);

        for (int i = 0; i < bytes_received; ++i) {
            buffer[i] = toupper((unsigned char)buffer[i]); }
        printf("- Child 1 converted to uppercase: '%s'\n", buffer);

        printf("- Child 1 sending uppercase message (Priority: %d)...\n", PRIORITY_CHILD1_TO_CHILD2);
        if (mq_send(child1_mq, buffer, bytes_received, PRIORITY_CHILD1_TO_CHILD2) == -1) {
            perror("Child 1 mq_send failed"); mq_close(child1_mq); exit(EXIT_FAILURE); }

        if (mq_close(child1_mq) == -1) {
            perror("Child 1 mq_close failed"); exit(EXIT_FAILURE); }
        printf("- Child 1 finished and closed queue.\n");
        exit(EXIT_SUCCESS);
    }

    /* --- Parent continues: Fork Child 2 --- */
    pid2 = fork();
    if (pid2 < 0) {
        perror("fork (child 2) failed");
        if (pid1 > 0) waitpid(pid1, &status1, 0);
        mq_close(mq);
        mq_unlink(QUEUE_NAME);
        exit(EXIT_FAILURE);
    }

    if (pid2 == 0) {
        /* ----- Child Process 2 ----- */
        mqd_t child2_mq;
        printf("- Child 2 (PID: %d) started.\n", getpid());
        child2_mq = mq_open(QUEUE_NAME, O_RDONLY);
        if (child2_mq == (mqd_t)-1) {
            perror("Child 2 mq_open failed"); exit(EXIT_FAILURE); }

        printf("- Child 2 waiting for uppercase message from Child 1...\n");
        bytes_received = mq_receive(child2_mq, buffer, MSG_BUFFER_SIZE, &received_priority);
        if (bytes_received == -1) {
            perror("Child 2 mq_receive failed"); mq_close(child2_mq); exit(EXIT_FAILURE); }

        buffer[bytes_received] = '\0';
        printf("- Child 2 received final message (Priority: %u): '%s'\n", received_priority, buffer);

         if (mq_close(child2_mq) == -1) {
            perror("Child 2 mq_close failed"); exit(EXIT_FAILURE); }
        printf("- Child 2 finished and closed queue.\n");
        exit(EXIT_SUCCESS);
    }

    /* ----- Parent Process ----- */
    /* Use the macro for the initial message content */
    const char *initial_message = MESSAGE_CONTENT; /* MODIFICATION HERE */

    printf("- Parent (PID: %d) sending initial message (Priority: %d): '%s'\n",
           getpid(), PRIORITY_PARENT_TO_CHILD1, initial_message);

    /* Send the initial message with lower priority */
    if (mq_send(mq, initial_message, strlen(initial_message), PRIORITY_PARENT_TO_CHILD1) == -1) {
        perror("Parent mq_send failed");
        waitpid(pid1, &status1, WNOHANG);
        waitpid(pid2, &status2, WNOHANG);
        mq_close(mq);
        mq_unlink(QUEUE_NAME);
        exit(EXIT_FAILURE);
    }

    /* Wait for both child processes to complete */
    printf("- Parent waiting for Child 1 (PID: %d)...\n", pid1);
    if (waitpid(pid1, &status1, 0) == -1) {
        perror("Parent waitpid for child 1 failed");
    } else {
        printf("- Parent detected Child 1 has finished.\n");
    }

    printf("- Parent waiting for Child 2 (PID: %d)...\n", pid2);
     if (waitpid(pid2, &status2, 0) == -1) {
        perror("Parent waitpid for child 2 failed");
    } else {
        printf("- Parent detected Child 2 has finished.\n");
    }

    /* Close and unlink the message queue */
    printf("- Parent closing queue descriptor.\n");
    if (mq_close(mq) == -1) {
        perror("Parent mq_close failed");
    }

    printf("- Parent unlinking queue '%s'.\n", QUEUE_NAME);
    if (mq_unlink(QUEUE_NAME) == -1) {
        perror("Parent mq_unlink failed");
    }

    printf("- Parent process finished.\n");
    exit(EXIT_SUCCESS);
}