#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "server.h"
#include "command_handler.h"

int connected_clients = 0;

void *handle_tcp_client(void *arg) {
    char buf[BUFFER_SIZE];
    int client_fd = (int)(intptr_t)arg;

    while (1) {
        memset(buf, 0, sizeof(buf));
        
        ssize_t bytes_received = recv(client_fd, buf, sizeof(buf), 0);
        if (bytes_received <= 0) {
            connected_clients--;
            printf("Closing connection for client %d, connected clients: %d\n", client_fd, connected_clients);
            close(client_fd);
            return NULL;
        }

        printf("Data received: %s", buf);
        
        command_response_t cmd_res = handle_command(buf);
        send(client_fd, cmd_res.response, strlen(cmd_res.response), 0);
        
        if (cmd_res.should_close) {
            connected_clients--;
            printf("Closing connection for client %d, connected clients: %d\n", client_fd, connected_clients);
            close(client_fd);
            return NULL;
        }
    }
}

void start_tcp_server() {
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(1234);
    serveraddr.sin_addr.s_addr = INADDR_ANY;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Server socket init failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Socketopt error");
        exit(EXIT_FAILURE);
    }

    if (bind(server_fd, (const struct sockaddr*) &serveraddr, sizeof(serveraddr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);   
    }

    if (listen(server_fd, 1) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);  
    }

    printf("Server listening on 0.0.0.0:1234\n");

    while (1) {
        struct sockaddr_in addr;
        socklen_t addr_len;

        int client_fd = accept(server_fd, (struct sockaddr*) &addr, &addr_len);
        if (client_fd < 0) {
            perror("Connection failed");
            continue;
        }

        connected_clients++;
        printf("Client connected, total connected clients: %d\n", connected_clients);

        pthread_t tid;
        pthread_create(&tid, NULL, handle_tcp_client, (void *)(intptr_t)client_fd);
        pthread_detach(tid);
    }

    close(server_fd);
}

