#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "MT25087_Part_A_A1_common.h"

#define QUEUE_LEN 128

typedef struct {
    int fds[QUEUE_LEN];
    int head, tail, count;
    pthread_mutex_t lock;
    pthread_cond_t ready;
} socket_queue_t;

static socket_queue_t sq;
static int field_bytes;

/* ------------ Queue ------------ */

void sq_init(socket_queue_t *q) {
    q->head = q->tail = q->count = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->ready, NULL);
}

void sq_push(socket_queue_t *q, int fd) {
    pthread_mutex_lock(&q->lock);

    q->fds[q->tail] = fd;
    q->tail = (q->tail + 1) % QUEUE_LEN;
    q->count++;

    pthread_cond_signal(&q->ready);
    pthread_mutex_unlock(&q->lock);
}

int sq_pop(socket_queue_t *q) {
    pthread_mutex_lock(&q->lock);

    while (q->count == 0)
        pthread_cond_wait(&q->ready, &q->lock);

    int fd = q->fds[q->head];
    q->head = (q->head + 1) % QUEUE_LEN;
    q->count--;

    pthread_mutex_unlock(&q->lock);
    return fd;
}

/* ------------ Worker ------------ */

void *worker_loop(void *arg) {

    message_t msg;
    msg.field_size = field_bytes;

    for (int i = 0; i < NUM_FIELDS; i++) {
        msg.fields[i] = malloc(msg.field_size);
        memset(msg.fields[i], 'A' + i, msg.field_size);
    }

    while (1) {

        int client = sq_pop(&sq);
        printf("[A1-worker] client connected\n");

        long sent = 0;
        struct timeval last, now;
        gettimeofday(&last, NULL);

        while (1) {

            for (int i = 0; i < NUM_FIELDS; i++) {
                ssize_t r = send(client,
                                 msg.fields[i],
                                 msg.field_size,
                                 0);
                if (r <= 0)
                    goto done;
            }

            sent++;

            gettimeofday(&now, NULL);
            if (now.tv_sec != last.tv_sec) {
                printf("[A1-worker] messages/sec = %ld\n", sent);
                sent = 0;
                last = now;
            }
        }

done:
        close(client);
        printf("[A1-worker] client closed\n");
    }
}

/* ------------ Main ------------ */

int main(int argc, char **argv) {

    if (argc != 4) {
        printf("Usage: %s <port> <msg_size> <workers>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    field_bytes = atoi(argv[2]);
    int workers = atoi(argv[3]);

    sq_init(&sq);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(listen_fd, 64);

    pthread_t pool[workers];

    for (int i = 0; i < workers; i++)
        pthread_create(&pool[i], NULL, worker_loop, NULL);

    printf("[A1-server] listening on %d with %d workers\n", port, workers);

    while (1) {
        int fd = accept(listen_fd, NULL, NULL);
        sq_push(&sq, fd);
    }
}
