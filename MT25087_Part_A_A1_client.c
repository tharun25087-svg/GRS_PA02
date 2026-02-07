#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <string.h>

static int establish_link(const char *ip, int port) {

    int s = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in peer;
    memset(&peer, 0, sizeof(peer));

    peer.sin_family = AF_INET;
    peer.sin_port = htons(port);
    inet_pton(AF_INET, ip, &peer.sin_addr);

    if (connect(s, (struct sockaddr *)&peer, sizeof(peer)) < 0) {
        perror("connect");
        exit(1);
    }

    return s;
}

double seconds_between(struct timeval *a, struct timeval *b) {
    return (b->tv_sec - a->tv_sec) +
           (b->tv_usec - a->tv_usec) / 1e6;
}

int main(int argc, char **argv) {

    if (argc != 5) {
        printf("Usage: %s <ip> <port> <msg_size> <runtime>\n", argv[0]);
        return 1;
    }

    const char *srv_ip = argv[1];
    int srv_port = atoi(argv[2]);
    int chunk = atoi(argv[3]);
    int runtime = atoi(argv[4]);

    int fd = establish_link(srv_ip, srv_port);

    printf("[A1-client] connected\n");

    char *buf = malloc(chunk);

    long total = 0;
    long slice = 0;

    struct timeval begin, current, last;
    gettimeofday(&begin, NULL);
    last = begin;

    while (1) {

        ssize_t r = read(fd, buf, chunk);
        if (r <= 0)
            break;

        total += r;
        slice += r;

        gettimeofday(&current, NULL);

        if (seconds_between(&last, &current) >= 1.0) {
            printf("[A1-client] bytes/sec = %ld\n", slice);
            slice = 0;
            last = current;
        }

        if (seconds_between(&begin, &current) >= runtime)
            break;
    }

    printf("[A1-client] finished — total bytes %ld\n", total);

    free(buf);
    close(fd);
    return 0;
}
