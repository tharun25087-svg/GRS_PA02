#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <string.h>

static int open_connection(const char *ip, int port) {

    int fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));

    dst.sin_family = AF_INET;
    dst.sin_port = htons(port);
    inet_pton(AF_INET, ip, &dst.sin_addr);

    if (connect(fd, (struct sockaddr *)&dst, sizeof(dst)) < 0) {
        perror("connect");
        exit(1);
    }

    return fd;
}

double diff_sec(struct timeval *a, struct timeval *b) {
    return (b->tv_sec - a->tv_sec) +
           (b->tv_usec - a->tv_usec) / 1000000.0;
}

int main(int argc, char **argv) {

    if (argc != 5) {
        printf("Usage: %s <ip> <port> <field_size> <seconds>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int field_size = atoi(argv[3]);
    int runtime = atoi(argv[4]);

    int fd = open_connection(server_ip, server_port);

    printf("[A2-client] connected\n");

    size_t chunk = field_size * 8;
    char *rx = malloc(chunk);

    long grand_total = 0;
    long window_total = 0;

    struct timeval start, now, last_tick;
    gettimeofday(&start, NULL);
    last_tick = start;

    while (1) {

        ssize_t n = read(fd, rx, chunk);
        if (n <= 0)
            break;

        grand_total += n;
        window_total += n;

        gettimeofday(&now, NULL);

        if (diff_sec(&last_tick, &now) >= 1.0) {
            printf("[A2-client] bytes/sec = %ld\n", window_total);
            window_total = 0;
            last_tick = now;
        }

        if (diff_sec(&start, &now) >= runtime)
            break;
    }

    printf("[A2-client] finished — total bytes %ld\n", grand_total);

    free(rx);
    close(fd);
    return 0;
}
