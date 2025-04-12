#include <stdio.h>      /* Standard I/O */
#include <stdlib.h>     /* Standard Library (exit, atoi, rand, etc.) */
#include <string.h>     /* String manipulation (memset, memcpy, strerror) */
#include <unistd.h>     /* POSIX API (write, close, sleep, usleep) */
#include <stdint.h>     /* For uint16_t */
#include <sys/socket.h> /* Socket programming */
#include <netinet/in.h> /* Internet address family structures */
#include <arpa/inet.h>  /* For inet_addr, htons, ntohs */
#include <netdb.h>      /* For gethostbyname */
#include <time.h>       /* For time() to seed rand() */
#include <errno.h>      /* For errno */
#include <limits.h>     /* For LONG_MAX, LONG_MIN */

/* --- Configuration --- */
#define BASE_TEMP 100.0  /* Base temperature for simulation */
#define TEMP_FLUCTUATION 5.0 /* Max +/- fluctuation from base temp */

/* --- Function Prototypes --- */
static void print_usage(const char *prog_name);
static double generate_temperature(void);

/* --- Main Function --- */

int main(int argc, char *argv[]) {
    if (argc != 5) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    /* 1. Parse Arguments */
    const char *server_ip_or_hostname = argv[1];
    char *endptr_port, *endptr_id, *endptr_interval;
    long port_long, id_long, interval_ms_long;
    int server_port, sensor_id_int, interval_ms;
    int client_sd = -1; /* Client socket descriptor */
    struct sockaddr_in server_addr;
    struct hostent *server_host;
    char send_buffer[sizeof(uint16_t) + sizeof(double)];
    uint16_t network_sensor_id;

    errno = 0;
    port_long = strtol(argv[2], &endptr_port, 10);
    if ((errno == ERANGE && (port_long == LONG_MAX || port_long == LONG_MIN)) || (errno != 0 && port_long == 0) || endptr_port == argv[2] || *endptr_port != '\0' || port_long < 1 || port_long > 65535) {
        fprintf(stderr, "Error: Invalid port number '%s'. Must be 1-65535.\n", argv[2]);
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    server_port = (int)port_long;

    errno = 0;
    id_long = strtol(argv[3], &endptr_id, 10);
    if ((errno == ERANGE && (id_long == LONG_MAX || id_long == LONG_MIN)) || (errno != 0 && id_long == 0) || endptr_id == argv[3] || *endptr_id != '\0' || id_long < 1 || id_long > 65535) {
        fprintf(stderr, "Error: Invalid sensor ID '%s'. Must be 1-65535.\n", argv[3]);
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    sensor_id_int = (int)id_long;
    network_sensor_id = htons((uint16_t)sensor_id_int); /* Convert ID to network byte order */

    errno = 0;
    interval_ms_long = strtol(argv[4], &endptr_interval, 10);
     if ((errno == ERANGE && (interval_ms_long == LONG_MAX || interval_ms_long == LONG_MIN)) || (errno != 0 && interval_ms_long == 0) || endptr_interval == argv[4] || *endptr_interval != '\0' || interval_ms_long < 10) { /* Min interval 10ms */
        fprintf(stderr, "Error: Invalid interval '%s'. Must be >= 10 milliseconds.\n", argv[4]);
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    interval_ms = (int)interval_ms_long;

    printf("INFO: Sensor Simulator started for Sensor ID: %d\n", sensor_id_int);
    printf("INFO: Connecting to %s:%d\n", server_ip_or_hostname, server_port);
    printf("INFO: Sending data every %d ms\n", interval_ms);

    /* Seed random number generator */
    srand(time(NULL) ^ getpid()); /* Add pid for better randomness if multiple instances start simultaneously */

    /* 2. Create Socket */
    client_sd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sd < 0) {
        perror("Error creating socket");
        return EXIT_FAILURE;
    }

    /* 3. Resolve Server Address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    /* Try interpreting as IP address first */
    if (inet_pton(AF_INET, server_ip_or_hostname, &server_addr.sin_addr) <= 0) {
        /* If not a valid IP, try resolving as hostname */
        printf("INFO: Resolving hostname '%s'...\n", server_ip_or_hostname);
        server_host = gethostbyname(server_ip_or_hostname);
        if (server_host == NULL || server_host->h_addr_list[0] == NULL) {
            fprintf(stderr, "Error: Could not resolve host '%s'\n", server_ip_or_hostname);
            close(client_sd);
            return EXIT_FAILURE;
        }
        /* Copy the first resolved address */
        memcpy(&server_addr.sin_addr, server_host->h_addr_list[0], sizeof(server_addr.sin_addr));
         printf("INFO: Hostname resolved to IP: %s\n", inet_ntoa(server_addr.sin_addr));
    }


    /* 4. Connect to Server */
    if (connect(client_sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error connecting to server");
        close(client_sd);
        return EXIT_FAILURE;
    }
    printf("INFO: Connected to server successfully.\n");

    /* 5. Send Data Loop */
    while (1) { /* Loop indefinitely until error or Ctrl+C */
        double current_temp = generate_temperature();
        
        /* Prepare data packet */
        memcpy(send_buffer, &network_sensor_id, sizeof(uint16_t));
        memcpy(send_buffer + sizeof(uint16_t), &current_temp, sizeof(double));

        /* Send data */
        ssize_t bytes_sent = write(client_sd, send_buffer, sizeof(send_buffer));

        if (bytes_sent == sizeof(send_buffer)) {
            printf("INFO: Sent Sensor ID: %d, Temp: %.2f\n", sensor_id_int, current_temp);
        } else if (bytes_sent < 0) {
             /* Error sending data */
             if (errno == EPIPE) {
                 fprintf(stderr, "ERROR: Server closed connection (Broken pipe). Exiting.\n");
             } else {
                 perror("Error sending data");
             }
             break; /* Exit loop on error */
        } else {
             /* Partial write - should ideally handle this, but exit for simplicity */
             fprintf(stderr, "ERROR: Partial write occurred (%zd bytes). Exiting.\n", bytes_sent);
             break;
        }

        /* Wait for the specified interval */
        /* usleep requires microseconds */
        if (usleep(interval_ms * 1000) == -1) {
             /* usleep interrupted (e.g., by Ctrl+C) */
             perror("WARN: usleep interrupted");
             break; /* Exit loop if sleep is interrupted */
        }; 
    }

    /* 6. Cleanup */
    printf("INFO: Sensor Simulator for ID %d shutting down.\n", sensor_id_int);
    if (client_sd != -1) {
        close(client_sd);
    }

    return EXIT_SUCCESS;
}

/**
 * @brief Prints command line usage instructions.
 * @param prog_name The name of the executable (argv[0]).
 */
static void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <server_ip_or_hostname> <port> <sensor_id> <interval_ms>\n", prog_name);
    fprintf(stderr, "  <server_ip_or_hostname>: IP address or hostname of the Sensor Gateway\n");
    fprintf(stderr, "  <port>                 : TCP port number of the Sensor Gateway (1-65535)\n");
    fprintf(stderr, "  <sensor_id>            : Unique ID for this sensor (1-65535)\n");
    fprintf(stderr, "  <interval_ms>          : Interval between readings in milliseconds (>= 10)\n");
}

/**
 * @brief Generates a simulated temperature reading.
 * @return A simulated temperature value.
 */
static double generate_temperature(void) {
    /* Generate random number between -1.0 and +1.0 */
    double fluctuation = ((double)rand() / (double)RAND_MAX) * 2.0 - 1.0; 
    return BASE_TEMP + fluctuation * TEMP_FLUCTUATION;
}