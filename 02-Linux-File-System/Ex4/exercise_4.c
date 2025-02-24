#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h> 
#include <string.h>

/* Get file type from mode */
const char *get_file_type(mode_t mode) {
    if (S_ISREG(mode)) return "Regular file";
    if (S_ISDIR(mode)) return "Directory";
    if (S_ISCHR(mode)) return "Character device";
    if (S_ISBLK(mode)) return "Block device";
    if (S_ISFIFO(mode)) return "FIFO/pipe";
    if (S_ISLNK(mode)) return "Symbolic link";
    return "Unknown";
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *filename = argv[1];

    /* Create file if not exist */
    FILE *fp = fopen(filename, "a");
    if (fp == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    fclose(fp);


    /* Get information of file */
    struct stat file_stat;
    if (stat(filename, &file_stat) == -1) {
        perror("stat");
        exit(EXIT_FAILURE);
    }


    /* Print information */
    printf("File Name: %s\n", filename);
    printf("File Type: %s\n", get_file_type(file_stat.st_mode));

    /* Change the frame of time stamp */
    char time_str[100];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&file_stat.st_mtime));
    printf("Last Modified Time: %s\n", time_str);

    printf("File Size: %lld bytes\n", (long long)file_stat.st_size);

    return 0;
}