#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "unix_transport.h"

static int server_fd;

void unix_init() {
    struct sockaddr_un addr;
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket initialization failed");
        exit(EXIT_FAILURE);
    }

    unlink(SOCKET_PATH);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (const struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("Socket binding failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Socket listen failed");
        exit(EXIT_FAILURE);
    }

    printf("[unix] Listening on %s\n", SOCKET_PATH);
}

int unix_accept() {
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) {
        perror("Accept failed");
    }
    return client_fd;
}

void unix_close() {
    close(server_fd);
    unlink(SOCKET_PATH);
    printf("[unix] Closed server and removed socket file\n");
}
