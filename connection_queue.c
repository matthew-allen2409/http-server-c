#include <stdio.h>
#include <string.h>
#include "connection_queue.h"

int connection_queue_init(connection_queue_t *queue) {
    queue->length = 0;
    queue->shutdown = 0;
    if (pthread_mutex_init(&queue->lock, NULL) != 0) {
        printf("Failed init\n");
        return -1;
    }
    if (pthread_cond_init(&queue->queue_empty, NULL) != 0) {
        printf("Failed init\n");
        return -1;
    }
    if (pthread_cond_init(&queue->queue_full, NULL) != 0) {
        printf("Failed init\n");
        return -1;
    }
    return 0;
}

int connection_enqueue(connection_queue_t *queue, int connection_fd) {
    pthread_mutex_lock(&queue->lock);
    // While at capacity and not in shutdown
    while (queue->length == CAPACITY && queue->shutdown == 0) {
        if (pthread_cond_wait(&queue->queue_full, &queue->lock) != 0) {
            printf("error in cond_wait\n");
            return -1;
        }
    }
    // Case of shutdown
    if (queue->shutdown != 0) {
        return -1;
    }
    queue->client_fds[queue->length] = connection_fd;
    queue->length = queue->length + 1;
    pthread_cond_signal(&queue->queue_empty);
    pthread_mutex_unlock(&queue->lock);
    return 0;
}

int connection_dequeue(connection_queue_t *queue) {
    pthread_mutex_lock(&queue->lock);
    while (queue->length == 0 && queue->shutdown == 0) {
        pthread_cond_wait(&queue->queue_empty, &queue->lock);
    }
    if (queue->shutdown != 0) {
        return -1;
    }
    int res_fd = queue->client_fds[queue->length - 1];
    queue->length = queue->length -1;
    pthread_cond_signal(&queue->queue_full);
    pthread_mutex_unlock(&queue->lock);
    return res_fd;
}

int connection_queue_shutdown(connection_queue_t *queue) {
    queue->shutdown = 1;
    pthread_cond_broadcast(&queue->queue_empty);
    pthread_cond_broadcast(&queue->queue_full);
    return 0;
}

int connection_queue_free(connection_queue_t *queue) {
    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->queue_empty);
    pthread_cond_destroy(&queue->queue_full);
    return 0;
}