/* cmd_client.c - Simple client to send commands to gateway */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define SOCKET_PATH "/command/sensor_gateway_cmd.sock" // Must match config.h
#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
    int sd;
    struct sockaddr_un server_addr;
    char buffer[BUFFER_SIZE];

    /* Check arguments */
    if (argc != 2 || (strcmp(argv[1], "status") != 0 && strcmp(argv[1], "stats") != 0)) {
        fprintf(stderr, "Usage: %s <status|stats>\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *command = argv[1];

    /* Create socket */
    sd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sd < 0) {
        perror("/* ERROR: socket() failed */");
        return EXIT_FAILURE;
    }

    /* Set server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    /* Connect to server */
    if (connect(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("/* ERROR: connect() failed. Is the gateway running? */");
        close(sd);
        return EXIT_FAILURE;
    }

    /* Send command */
    if (write(sd, command, strlen(command)) < 0) {
        perror("/* ERROR: write() failed */");
        close(sd);
        return EXIT_FAILURE;
    }
    printf("/* Sent command: %s */\n", command);

     /* Shutdown write end to signal end of command */
    shutdown(sd, SHUT_WR);

    /* Read response */
    printf("/* --- Gateway Response --- */\n");
    ssize_t bytes_read;
    while ((bytes_read = read(sd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0'; /* Null-terminate */
        printf("%s", buffer);      /* Print response chunk */
    }

    if (bytes_read < 0) {
        perror("/* ERROR: read() failed */");
    }
    printf("/* --- End of Response --- */\n");

    /* Close socket */
    close(sd);

    return EXIT_SUCCESS;
}