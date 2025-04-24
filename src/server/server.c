#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "server.h"
#include "command_handler.h"

#define MAX_FOLLOWERS 10

static config_t server_config;
int connected_clients = 0;

int followers[MAX_FOLLOWERS] = {0};

void set_server_config(const config_t *config) {
    memcpy(&server_config, config, sizeof(config_t));
}

static const char *get_shared_secret() {
    const char *secret = getenv("KEYVAL_SECRET");
    if (!secret) {
        fprintf(stderr, "Could not get secret, make sure KEYVAL_SECRET environment variable is set.\n");
        exit(EXIT_FAILURE);
    }

    return secret;
}

static void add_follower_to_list(int socket_fd) {
    char handshake_res[128] = {0};
    for (int i = 0; i < MAX_FOLLOWERS; i++) {
        if (followers[i] == 0) {
            followers[i] = socket_fd;
            strcpy(handshake_res, "OK\n");
            send(socket_fd, handshake_res, sizeof(handshake_res), 0);
            return;
        }
    }

    strcpy(handshake_res, "MAX_SIZE_EXCEEDED\n");
    send(socket_fd, handshake_res, sizeof(handshake_res), 0);
    printf("Max size exceeded, closing follower connection request\n");
    close(socket_fd);
}

static void broadcast_to_followers(const char *message) {
    for (int i = 0; i < MAX_FOLLOWERS; i++) {
        if (followers[i] != 0) {
            send(followers[i], message, strlen(message), 0);
        }
    }
}

static void drop_connection_attempt(int socket_fd, const char *reason) {
    connected_clients--;
    printf("Dropping connection for client %d, connected clients: %d\n", socket_fd, connected_clients);
    send(socket_fd, reason, strlen(reason), 0);
    close(socket_fd);
}

void *handle_client(void *arg) {
    char buf[BUFFER_SIZE];
    char handshake_msg[128];
    int client_fd = (int)(intptr_t) arg;

    // Check for handshake message
    ssize_t bytes_received = recv(client_fd, handshake_msg, sizeof(handshake_msg), 0);
    if (bytes_received <= 0) {
        connected_clients--;
        printf("Closing connection for client %d, connected clients: %d\n", client_fd, connected_clients);
        close(client_fd);
        return NULL;
    }

    if (strncmp("HELLO CLIENT\n", handshake_msg, 13) == 0) {
        printf("Client handshake completed\n");
    } else if (strncmp("HELLO FOLLOWER|", handshake_msg, 15) == 0) {
        const char *follower_secret = get_shared_secret();
        char *token = strchr(handshake_msg, '|') + 1;
        token[strcspn(token, "\n")] = '\0';
        if (strcmp(token, follower_secret) == 0) {
            printf("Follower authenticated, handshake completed\n");
            add_follower_to_list(client_fd);
            return NULL;
        } else {
            printf("Follower rejected: invalid secret key\n");
            drop_connection_attempt(client_fd, "INVALID_SECRET_KEY\n");
            return NULL;
        }
    } else {
        printf("Handshake failed, invalid message: %s\n", handshake_msg);
        drop_connection_attempt(client_fd, "INVALID_HANDSHAKE_MESSAGE\n");
        return NULL;
    }

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

        if (cmd_res.type == CLOSE) {
            connected_clients--;
            printf("Closing connection for client %d, connected clients: %d\n", client_fd, connected_clients);
            close(client_fd);
            return NULL;
        }

        // check if it's a write operation and broadcast to followers from leader
        if (cmd_res.type == PUT || cmd_res.type == DELETE) {
            if (strcmp(server_config.role, "leader") != 0) {
                strcpy(cmd_res.response, "WRITE_NOT_ALLOWED\n");
                send(client_fd, cmd_res.response, strlen(cmd_res.response), 0);
                continue;
            }
            broadcast_to_followers(buf);
        }

        send(client_fd, cmd_res.response, strlen(cmd_res.response), 0);
    }
}

static void start_tcp_server(int port) {
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
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

    printf("Server listening on 0.0.0.0:%d\n", port);

    while (1) {
        struct sockaddr_in addr;
        socklen_t addr_len;

        int client_fd = accept(server_fd, (struct sockaddr*) &addr, &addr_len);
        if (client_fd < 0) {
            perror("Connection failed");
            continue;
        }

        connected_clients++;
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, client_ip, sizeof(client_ip));
        printf("Client %s:%d connected, total connected clients: %d\n", client_ip, ntohs(addr.sin_port), connected_clients);

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, (void *)(intptr_t) client_fd);
        pthread_detach(tid);
    }

    close(server_fd);
}

void start_leader_server(int port) {
    start_tcp_server(port);
}

int connect_with_leader_server(const char *leader_host, int leader_port) {
    char handshake_buf[128] = {0};
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(leader_port);

    if (inet_pton(AF_INET, leader_host, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/address not supported");
        exit(EXIT_FAILURE);
    }

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("Socket connection failed");
        exit(EXIT_FAILURE);
    }

    if (connect(socket_fd, (const struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // Initiate handshake
    printf("Initiating handshake with leader...\n");
    const char *follower_secret = get_shared_secret();
    snprintf(handshake_buf, sizeof(handshake_buf), "HELLO FOLLOWER|%s\n", follower_secret);
    send(socket_fd, handshake_buf, sizeof(handshake_buf), 0);

    memset(handshake_buf, 0, sizeof(handshake_buf));
    recv(socket_fd, handshake_buf, sizeof(handshake_buf), 0);
    if (strcmp("OK\n", handshake_buf) != 0) {
        fprintf(stderr, "Failed to connect with leader shutting down. Reason: %s", handshake_buf);
        exit(EXIT_FAILURE);
    }

    printf("Leader handshake successful.\n");
    return socket_fd;
}

static void *handle_leader(void *arg) {
    int socket_fd = (int)(intptr_t) arg;
    char buf[BUFFER_SIZE];
    printf("Opening communication channel with leader\n");
    while (1) {
        memset(buf, 0, BUFFER_SIZE);
        ssize_t bytes_received = recv(socket_fd, buf, sizeof(buf), 0);
        if (bytes_received <= 0) {
            perror("Closing connection with leader");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }
        
        printf("Received msg from leader: %s\n", buf);
    }
    return NULL;
}

void start_follower_server(int port, const char *leader_host, int leader_port) {
    int leader_socket_fd = connect_with_leader_server(leader_host, leader_port);

    // Spawn new thread for leader communication
    pthread_t tid;
    pthread_create(&tid, NULL, handle_leader, (void *)(intptr_t) leader_socket_fd);
    pthread_detach(tid);

    start_tcp_server(port);
    close(leader_socket_fd);
}
