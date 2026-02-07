#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/time.h>

#include "MT25087_Part_A_A1_common.h"

#define QSIZE 128

typedef struct {
    int buf[QSIZE];
    int head, tail, count;
    pthread_mutex_t mtx;
    pthread_cond_t cv;
} fd_queue_t;

static fd_queue_t queue;
static int field_bytes;

/* ---------------- Queue ---------------- */

void q_init(fd_queue_t *q) {
    q->head = q->tail = q->count = 0;
    pthread_mutex_init(&q->mtx, NULL);
    pthread_cond_init(&q->cv, NULL);
}

void q_push(fd_queue_t *q, int fd) {
    pthread_mutex_lock(&q->mtx);
    q->buf[q->tail] = fd;
    q->tail = (q->tail + 1) % QSIZE;
    q->count++;
    pthread_cond_signal(&q->cv);
    pthread_mutex_unlock(&q->mtx);
}

int q_pop(fd_queue_t *q) {
    pthread_mutex_lock(&q->mtx);
    while (q->count == 0)
        pthread_cond_wait(&q->cv, &q->mtx);

    int fd = q->buf[q->head];
    q->head = (q->head + 1) % QSIZE;
    q->count--;
    pthread_mutex_unlock(&q->mtx);
    return fd;
}

/* ---------------- Worker ---------------- */

void *worker(void *unused) {

    message_t packet;
    packet.field_size = field_bytes;

    struct iovec vec[NUM_FIELDS];
    struct msghdr hdr;
    memset(&hdr, 0, sizeof(hdr));

    for (int i = 0; i < NUM_FIELDS; i++) {
        packet.fields[i] = malloc(packet.field_size);
        memset(packet.fields[i], 'A' + i, packet.field_size);
        vec[i].iov_base = packet.fields[i];
        vec[i].iov_len = packet.field_size;
    }

    hdr.msg_iov = vec;
    hdr.msg_iovlen = NUM_FIELDS;

    while (1) {

        int client = q_pop(&queue);
        printf("[A2-worker] client connected\n");

        long sent = 0;
        struct timeval t0, t1;
        gettimeofday(&t0, NULL);

        while (1) {
            ssize_t r = sendmsg(client, &hdr, 0);
            if (r <= 0) break;

            sent++;

            gettimeofday(&t1, NULL);
            if (t1.tv_sec != t0.tv_sec) {
                printf("[A2-worker] messages/sec = %ld\n", sent);
                sent = 0;
                t0 = t1;
            }
        }

        close(client);
        printf("[A2-worker] client closed\n");
    }
}

/* ---------------- Main ---------------- */

int main(int argc, char **argv) {

    if (argc != 4) {
        printf("Usage: %s <port> <msg_size> <workers>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    field_bytes = atoi(argv[2]);
    int workers = atoi(argv[3]);

    q_init(&queue);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(listen_fd, 64);

    pthread_t pool[workers];

    for (int i = 0; i < workers; i++)
        pthread_create(&pool[i], NULL, worker, NULL);

    printf("[A2-server] listening on %d with %d workers\n", port, workers);

    while (1) {
        int fd = accept(listen_fd, NULL, NULL);
        q_push(&queue, fd);
    }
}
