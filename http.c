#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include "http.h"

#define BUFSIZE 512

const char *get_mime_type(const char *file_extension) {
    if (strcmp(".txt", file_extension) == 0) {
        return "text/plain";
    } else if (strcmp(".html", file_extension) == 0) {
        return "text/html";
    } else if (strcmp(".jpg", file_extension) == 0) {
        return "image/jpeg";
    } else if (strcmp(".png", file_extension) == 0) {
        return "image/png";
    } else if (strcmp(".pdf", file_extension) == 0) {
        return "application/pdf";
    }

    return NULL;
}

int read_http_request(int fd, char *resource_name) {
    // TODO Cleanup
    FILE *sock = fdopen(fd, "r");
    if (sock == NULL) {
        perror("fdopen");
        return 1;
    }

    char buff[BUFSIZE];
    memset(buff, 0, sizeof(buff));
    while (fgets(buff, BUFSIZE, sock) != NULL) {
        if (strcmp(buff, "\r\n") == 0)
            break;
        if (strcmp(strtok(buff, " "), "GET") == 0) {
            strcpy(resource_name, strtok(NULL, " "));
        }
    }
    return 0;
}

int write_http_response(int fd, const char *resource_path) {
    // TODO Cleanup
    struct stat statbuf;
    if (stat(resource_path, &statbuf) == -1) {
        char response[] = "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        write(fd, response, sizeof(response));
        return 1;
    }

    write(fd, "HTTP/1.0 200 OK\r\n", 17);
    const char *extension = strchr(resource_path, '.');
    if (extension == NULL) {
        printf("extesnion is null\n");
        return 1;
    }
    char type[BUFSIZE];
    memset(type, 0, sizeof(type));
    strcpy(type, get_mime_type(extension));
    if (write(fd, "Content-Type: ", 14) == -1) {
        perror("write");
        return 1;
    }
    if (write(fd, type, strlen(type)) == -1) {
        perror("write");
        return 1;
    }
    if (write(fd, "\r\n", 2) == -1) {
        perror("write");
        return 1;
    }
    if (write(fd, "Content-Length: ", 16) == -1) {
        perror("write");
        return 1;
    }
    char length[BUFSIZE];
    memset(length, 0, sizeof(length));
    if (sprintf(length, "%ld", statbuf.st_size) < 0) {
        printf("sprintf Failed\n");
        return 1;
    }
    if (write(fd, length, strlen(length)) == -1) {
        perror("write");
        return 1;
    }
    if (write(fd, "\r\n\r\n", 4) == -1) {
        perror("write");
        return 1;
    }
    int file = open(resource_path, O_RDONLY);
    char buf[BUFSIZE];
    int bytes_read;
    while((bytes_read = read(file, buf, sizeof(buf))) > 0) {
        if (write(fd, buf, bytes_read) == -1) {
            perror("write");
            return 1;
        }
    }

    return 0;
}
