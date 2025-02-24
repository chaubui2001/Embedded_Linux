#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main() {
    const char *filename = "test.txt";
    const char *data_to_write = "No data found";
    const char *data_after_seek = "Exercise 1";

    /* Open file */
    int fd = open(filename, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    /* Write initial data to file */
    if (write(fd, data_to_write, strlen(data_to_write)) == -1) {
        perror("write (initial)");
        close(fd);
        exit(EXIT_FAILURE);
    }
    printf("Initial data written\n");

    /* Seek to begin of file */
    if (lseek(fd, 0, SEEK_SET) == -1) {
        perror("lseek");
        close(fd);
        exit(EXIT_FAILURE);
    }
    printf("Seeked to the beginning.\n");

    /* Write data after seeking */
    if (write(fd, data_after_seek, strlen(data_after_seek)) == -1) {
        perror("write (after seek)");
        close(fd);
        exit(EXIT_FAILURE);
    }

     printf("Data written at the beginning.\n");

    if (close(fd) == -1) {
        perror("close");
        exit(EXIT_FAILURE);
    }
    printf("File closed.\n");

    return 0;
}