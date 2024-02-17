#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "connection_queue.h"
#include "http.h"

#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5
#define N_THREADS 5

int keep_going = 1;
const char *serve_dir;

void handle_sigint(int signo) {
    keep_going = 0;
}

typedef struct {
    connection_queue_t *queue;
    char* dir;
} args_t;

void* thread_func(void* arg) {
    args_t *args = (args_t *) arg;

    int fd = connection_dequeue(args->queue);
    if (fd == -1) {
        // TODO ERROR HANDLING
        return NULL;
    }

    char resource_name[BUFSIZE];
    if (read_http_request(fd, resource_name) == 1) {
        printf("Failed to read request\n");
        if (close(fd) != 0) {
            perror("close");
            return NULL;
        }
    }

    char filepath[BUFSIZE];
    strcpy(filepath, args->dir);
    strcat(filepath, resource_name);
    write_http_response(fd, filepath);

    if (close(fd) != 0) {
            perror("close");
            return NULL;
    }
    
    return NULL;
}



int main(int argc, char **argv) {
    // First command is directory to serve, second command is port
    if (argc != 3) {
        printf("Usage: %s <directory> <port>\n", argv[0]);
        return 1;
    }

    struct sigaction act;
    act.sa_handler = handle_sigint;
    sigfillset(&act.sa_mask);
    act.sa_flags = 0;
    if (sigaction(SIGINT, &act, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    connection_queue_t queue;
    if (connection_queue_init(&queue) == -1 ) {
        printf("Failed to init queue");
        return 1;
    }

    char* dir = argv[1];
    char* port = argv[2];

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    struct addrinfo *server;

    int addrinfo = getaddrinfo(NULL, port, &hints, &server);
    if (addrinfo != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addrinfo));
        connection_queue_shutdown(&queue);
        return 1;
    }

    int sockfd = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if (sockfd == -1) {
        perror("socket");
        freeaddrinfo(server);
        connection_queue_shutdown(&queue);
        return 1;
    }

    if (bind(sockfd, server->ai_addr, server->ai_addrlen) == -1) {
        perror("bind");
        freeaddrinfo(server);
        close(sockfd);
        connection_queue_shutdown(&queue);
        return 1;
    }
    freeaddrinfo(server);

    struct sockaddr clientAddr;
    socklen_t clientAddrLen = sizeof(struct sockaddr);

    pthread_t threads[N_THREADS];

    args_t thread_args[N_THREADS];

    while (keep_going) {

        for (int i = 0; i < N_THREADS; i++) {
            thread_args[i].queue = &queue;
            thread_args[i].dir = dir;
            if (pthread_create(threads + i, NULL, thread_func, thread_args + i) != 0) {
                printf("Failed to create thread\n");
                close(sockfd);
                connection_queue_shutdown(&queue);
                return 1;
            }
        }

        if (listen(sockfd, LISTEN_QUEUE_LEN) == -1) {
            if (errno == EINTR) {
                // Server received SIGINT, so break out of loop
                break;
            } else {
                perror("listen");
                close(sockfd);
                return 1;
            }
        }

        int clientfd = accept(sockfd, &clientAddr, &clientAddrLen);
        if (clientfd == -1) {
            if (errno == EINTR) {
                break;
            }
            perror("accept");
            close(sockfd);
            return 1;
        }

        if (connection_enqueue(&queue, clientfd) == -1) {
            printf("Failed to enqueue");
            close(sockfd);
            connection_queue_shutdown(&queue);
            return 1;
        }
    }

    if (close(sockfd) == -1) {
        perror("close");
        return 1;
    }
    if (connection_queue_shutdown(&queue) == -1) {
        printf("Failed to close connection queue\n");
        return 1;
    }
    return 0;
}
