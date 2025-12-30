#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    printf("argc: %d, argv: %s", argc, argv[0]);

    int inputFd;
    int outputFd;
    char* buff = NULL;

    inputFd = open(argv[1], O_RDONLY);
    if (inputFd == -1) {
        perror("open inputFd");
        return -1;
    }

    outputFd = open(argv[2], O_CREAT|O_RDWR|O_TRUNC);
    if (outputFd == -1) {
        perror("open outputFd");
        return -1;
    }

    buff = malloc(4096);

    size_t n = read(inputFd, buff, 10);
    printf("buffer is: %s, rem ad: %zu", buff, n);
    write(outputFd, buff, n);

    close(inputFd);
    close(outputFd);
    return 0;
}
