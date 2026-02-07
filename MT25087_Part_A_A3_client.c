#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <string.h>

static int connect_to_server(const char *ip, int port) {

    int fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in remote;
    memset(&remote, 0, sizeof(remote));

    remote.sin_family = AF_INET;
    remote.sin_port = htons(port);
    inet_pton(AF_INET, ip, &remote.sin_addr);

    if (connect(fd, (struct sockaddr *)&remote, sizeof(remote)) < 0) {
        perror("connect");
        exit(1);
    }

    return fd;
}

double elapsed_sec(struct timeval *a, struct timeval *b) {
    return (b->tv_sec - a->tv_sec) +
           (b->tv_usec - a->tv_usec) / 1e6;
}

int main(int argc, char **argv) {

    if (argc != 5) {
        printf("Usage: %s <ip> <port> <field_size> <runtime>\n", argv[0]);
        return 1;
    }

    const char *remote_ip = argv[1];
    int remote_port = atoi(argv[2]);
    int field_bytes = atoi(argv[3]);
    int run_time = atoi(argv[4]);

    int socket_fd = connect_to_server(remote_ip, remote_port);

    printf("[Client] connected\n");

    size_t block = field_bytes * 8;
    char *buffer = calloc(1, block);

    long total = 0;
    long interval = 0;

    struct timeval t0, t1, tick;
    gettimeofday(&t0, NULL);
    tick = t0;

    while (1) {

        ssize_t n = read(socket_fd, buffer, block);
        if (n <= 0) break;

        total += n;
        interval += n;

        gettimeofday(&t1, NULL);

        if (elapsed_sec(&tick, &t1) >= 1.0) {
            printf("[Client] interval bytes = %ld\n", interval);
            interval = 0;
            tick = t1;
        }

        if (elapsed_sec(&t0, &t1) >= run_time)
            break;
    }

    printf("[Client] finished — total bytes %ld\n", total);

    free(buffer);
    close(socket_fd);
    return 0;
}

