#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <time.h>

#include "MT25087_Part_A_A1_common.h"

#define QUEUE_SIZE 128

typedef struct {
    int fds[QUEUE_SIZE];
    int head, tail, count;
    pthread_mutex_t lock;
    pthread_cond_t ready;
} fd_queue_t;

fd_queue_t client_queue;

int payload_bytes;

/* ---------------- Queue helpers ---------------- */

void queue_init(fd_queue_t *q) {
    q->head = q->tail = q->count = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->ready, NULL);
}

void queue_push(fd_queue_t *q, int fd) {
    pthread_mutex_lock(&q->lock);

    q->fds[q->tail] = fd;
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    q->count++;

    pthread_cond_signal(&q->ready);
    pthread_mutex_unlock(&q->lock);
}

int queue_pop(fd_queue_t *q) {
    pthread_mutex_lock(&q->lock);

    while (q->count == 0)
        pthread_cond_wait(&q->ready, &q->lock);

    int fd = q->fds[q->head];
    q->head = (q->head + 1) % QUEUE_SIZE;
    q->count--;

    pthread_mutex_unlock(&q->lock);
    return fd;
}

/* ------------ Zerocopy completion drain ------------ */

void reap_zerocopy(int fd) {
    char buf[256];
    struct msghdr m = {0};
    struct iovec iov = { buf, sizeof(buf) };
    char cbuf[512];

    m.msg_iov = &iov;
    m.msg_iovlen = 1;
    m.msg_control = cbuf;
    m.msg_controllen = sizeof(cbuf);

    while (recvmsg(fd, &m, MSG_ERRQUEUE | MSG_DONTWAIT) > 0);
}

/* ---------------- Worker Thread ---------------- */

void *worker_loop(void *arg) {

    message_t packet;
    packet.field_size = payload_bytes;

    struct iovec vectors[NUM_FIELDS];
    struct msghdr hdr = {0};

    for (int i = 0; i < NUM_FIELDS; i++) {
        packet.fields[i] = malloc(packet.field_size);
        memset(packet.fields[i], 'A' + i, packet.field_size);
        vectors[i].iov_base = packet.fields[i];
        vectors[i].iov_len = packet.field_size;
    }

    hdr.msg_iov = vectors;
    hdr.msg_iovlen = NUM_FIELDS;

    while (1) {
        int client = queue_pop(&client_queue);
        printf("[Worker] client accepted\n");

        long sent = 0;
        time_t prev = time(NULL);

        while (1) {
            ssize_t r = sendmsg(client, &hdr, MSG_ZEROCOPY);
            if (r <= 0) break;

            sent++;
            reap_zerocopy(client);

            time_t now = time(NULL);
            if (now != prev) {
                printf("[Worker] msgs/sec: %ld\n", sent);
                prev = now;
            }
        }

        close(client);
        printf("[Worker] client closed\n");
    }
}

/* ---------------- Main ---------------- */

int main(int argc, char **argv) {

    if (argc != 4) {
        printf("Usage: %s <port> <msg_size> <workers>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    payload_bytes = atoi(argv[2]);
    int workers = atoi(argv[3]);

    queue_init(&client_queue);

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    int one = 1;
    setsockopt(sock, SOL_SOCKET, SO_ZEROCOPY, &one, sizeof(one));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(sock, 64);

    pthread_t pool[workers];

    for (int i = 0; i < workers; i++)
        pthread_create(&pool[i], NULL, worker_loop, NULL);

    printf("[Server] listening on %d with %d workers\n", port, workers);

    while (1) {
        int client = accept(sock, NULL, NULL);
        setsockopt(client, SOL_SOCKET, SO_ZEROCOPY, &one, sizeof(one));
        queue_push(&client_queue, client);
    }
}
