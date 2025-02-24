#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s filename num-bytes [r/w/rw] \"data\"\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *filename = argv[1];
    char *endptr;
    long num_bytes = strtol(argv[2], &endptr, 10);
    char *read_write_mode = argv[3];
    const char *data = argv[4];

    if (*endptr != '\0' || num_bytes <= 0) {
        fprintf(stderr, "Invalid num-bytes: %s\n", argv[2]);
        exit(EXIT_FAILURE);
    }

    /* Check the validity of mode */
    if (strcmp(read_write_mode, "r") != 0 &&
        strcmp(read_write_mode, "w") != 0 &&
        strcmp(read_write_mode, "rw") != 0) {
        fprintf(stderr, "Invalid mode: %s. Must be 'r', 'w', or 'rw'.\n", read_write_mode);
        exit(EXIT_FAILURE);
    }

    int fd;

    /* Write to file */
    if (strcmp(read_write_mode, "w") == 0 || strcmp(read_write_mode, "rw") == 0) {
        fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            perror("open (write)");
            exit(EXIT_FAILURE);
        }

        ssize_t bytes_written = write(fd, data, num_bytes);
        if (bytes_written == -1) {
            perror("write");
            close(fd);
            exit(EXIT_FAILURE);
        }
        if (bytes_written < num_bytes) {
             fprintf(stderr, "Warning: Wrote only %zd of %ld bytes.\n", bytes_written, num_bytes);
        }
        printf("Data written to file.\n");

        if(strcmp(read_write_mode, "w") == 0){ /* If only write mode => return with reading file */
          close(fd);
          return 0;
        }
    }


    /* Read file */
    if (strcmp(read_write_mode, "r") == 0 || strcmp(read_write_mode, "rw") == 0) {

        /*if rw => need to close before open */
        if(strcmp(read_write_mode, "rw") == 0) {
            if (close(fd) == -1) { 
                perror("close (after write)");
                exit(EXIT_FAILURE);
              }
              fd = open(filename, O_RDONLY);
        }
        else{
           fd = open(filename, O_RDONLY);
        }

        if (fd == -1) {
            perror("open (read)");
            exit(EXIT_FAILURE);
        }

        char *buffer = (char *)malloc(num_bytes + 1);
        if (buffer == NULL) {
            perror("malloc");
            close(fd);
            exit(EXIT_FAILURE);
        }

        ssize_t bytes_read = read(fd, buffer, num_bytes);
        if (bytes_read == -1) {
            perror("read");
            free(buffer);
            close(fd);
            exit(EXIT_FAILURE);
        }

        buffer[bytes_read] = '\0';
        printf("Read data: \n%s\n", buffer);
        free(buffer);
    }



    /* Close file */
    if (close(fd) == -1) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    return 0;
}