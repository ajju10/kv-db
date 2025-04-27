#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include "server.h"
#include "command_handler.h"
#include "../logs/log.h"

#define MAX_FOLLOWERS 10

typedef struct follower {
    int id;
    int fd;
} follower_t;

static void sync_with_leader(int socket_fd, const char *log_path);
static void start_leader_handshake(int socket_fd);
static void start_follower_handshake(int socket_fd, int follower_id);

static config_t server_config;
int connected_clients = 0;

follower_t followers[MAX_FOLLOWERS] = {0};

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

static int validate_follower_connection(int id, const char *token) {
    for (int i = 0; i < MAX_FOLLOWERS; i++) {
        if (followers[i].id == id) {
            printf("Follower connection rejected: ID %d is already taken\n", id);
            return -1;
        }
    }

    const char *follower_secret = get_shared_secret();
    if (strncmp(token, follower_secret, strlen(follower_secret)) != 0) {
        printf("Follower connection rejected: Invalid token\n");
        return -2;
    }

    return 0;
}

static void send_logs_to_follower(int socket_fd) {
    char log_path[128];
    char buf[BUFFER_SIZE] = {0};
    snprintf(log_path, sizeof(log_path), "%s/%s", DATA_DIRECTORY, LEADER_DATA_FILE);

    // receive SYNC_READY message
    ssize_t bytes_received = recv(socket_fd, buf, sizeof(buf), 0);
    if (bytes_received <= 0 || strncmp(buf, "SYNC_READY\n", 11) != 0) {
        fprintf(stderr, "Failed to receive SYNC_READY message from follower\n");
        close(socket_fd);
        return;
    }
    printf("Received SYNC_READY message from follower, starting sync...\n");

    // Send SYNC_START message
    memset(buf, 0, sizeof(buf));
    strcpy(buf, "SYNC_START\n");
    send(socket_fd, buf, strlen(buf), 0);

    FILE *log_file = fopen(log_path, "r");
    if (log_file == NULL) {
        printf("No leader log file exists yet, sending SYNC_FINISHED\n");
        strcpy(buf, "SYNC_FINISHED\n");
        send(socket_fd, buf, strlen(buf), 0);
        return;
    }

    // Read and send each line from the log file
    while (fgets(buf, sizeof(buf), log_file)) {
        send(socket_fd, buf, strlen(buf), 0);
    }

    fclose(log_file);

    // Send SYNC_FINISHED message
    strcpy(buf, "SYNC_FINISHED\n");
    send(socket_fd, buf, strlen(buf), 0);
    printf("Finished sending log file to follower\n");
}

static void add_follower_to_list(int id, int socket_fd) {
    for (int i = 0; i < MAX_FOLLOWERS; i++) {
        if (followers[i].id == 0) {
            follower_t follower_conf = {0};
            follower_conf.id = id;
            follower_conf.fd = socket_fd;
            followers[i] = follower_conf;
            start_leader_handshake(socket_fd);
            return;
        }
    }

    char handshake_res[128] = {0};
    strcpy(handshake_res, "MAX_SIZE_EXCEEDED\n");
    send(socket_fd, handshake_res, sizeof(handshake_res), 0);
    printf("Max size exceeded, closing follower connection request\n");
    close(socket_fd);
}

static void broadcast_to_followers(const char *message) {
    for (int i = 0; i < MAX_FOLLOWERS; i++) {
        if (followers[i].id != 0) {
            send(followers[i].fd, message, strlen(message), 0);
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
        send(client_fd, "HELLO SERVER\n", 13, 0);
        printf("Client handshake completed\n");
    } else if (strncmp("HELLO FOLLOWER|", handshake_msg, 15) == 0) {
        if (strcmp(server_config.role, "follower") == 0) {
            printf("I am not a leader, cannot accept follower connection\n");
            drop_connection_attempt(client_fd, "CANNOT_ACCEPT_FOLLOWER_CONNECTION\n");
            return NULL;
        }

        char *hello_msg = strchr(handshake_msg, '|') + 1;
        int follower_id = 0;
        while (*hello_msg && *hello_msg != ':') {
            follower_id = follower_id * 10 + (*hello_msg - '0');
            hello_msg++;
        }
        hello_msg++;

        char *token = hello_msg;
        token[strcspn(token, "\n")] = '\0';
        int validation_result = validate_follower_connection(follower_id, token);
        if (validation_result == 0) {
            printf("Follower authenticated, handshake completed\n");
            add_follower_to_list(follower_id, client_fd);
            return NULL;
        } else if (validation_result == -1) {
            printf("Follower rejected: ID already taken\n");
            drop_connection_attempt(client_fd, "ID_ALREADY_TAKEN\n");
            return NULL;
        } else if (validation_result == -2) {
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
        char response[BUFFER_SIZE] = {0};

        if (cmd_res.type == CLOSE) {
            send(client_fd, "BYE\n", 4, 0);
            connected_clients--;
            printf("Closing connection for client %d, connected clients: %d\n", client_fd, connected_clients);
            close(client_fd);
            return NULL;
        }

        if (cmd_res.type == INVALID) {
            send(client_fd, "INVALID_COMMAND\n", 16, 0);
            continue;
        }

        if (cmd_res.type == PING) {
            send(client_fd, "PONG\n", 5, 0);
            connected_clients--;
            printf("Closing connection for client %d, connected clients: %d\n", client_fd, connected_clients);
            close(client_fd);
            return NULL;
        }

        // check if it's a write operation and validate against server role
        if (cmd_res.type == PUT || cmd_res.type == DELETE) {
            if (strcmp(server_config.role, "leader") != 0) {
                send(client_fd, "WRITE_NOT_ALLOWED\n", 18, 0);
                continue;
            }

            write_to_data_file(buf);
            broadcast_to_followers(buf);

            if (cmd_res.type == PUT) {
                if (kv_put(cmd_res.data.key, cmd_res.data.value) != 0) {
                    strcpy(response, "ERROR\n");
                } else {
                    strcpy(response, "OK\n");
                }
            } else {
                if (kv_delete(cmd_res.data.key) == 1) {
                    strcpy(response, "NOT_FOUND\n");
                } else {
                    strcpy(response, "OK\n");
                }
            }
        } else if (cmd_res.type == GET) {
            char *value = kv_get(cmd_res.data.key);
            if (value[0] == '\0') {
                strcpy(response, "NOT_FOUND\n");
            } else {
                snprintf(response, BUFFER_SIZE, "%s\n", value);
            }
        }

        send(client_fd, response, strlen(response), 0);
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
    mkdir(DATA_DIRECTORY, 0755);

    char log_path[128];
    snprintf(log_path, sizeof(log_path), "%s/%s", DATA_DIRECTORY, LEADER_DATA_FILE);
    replay_data_file(log_path);
    open_data_file(log_path);

    start_tcp_server(port);
}

static void sync_with_leader(int socket_fd, const char *log_path) {
    printf("Syncing with leader...\n");

    char temp_log_file[128];
    snprintf(temp_log_file, sizeof(temp_log_file), "%s.tmp", log_path);

    FILE *temp_log = fopen(temp_log_file, "w");
    if (temp_log == NULL) {
        perror("Error syncing with leader");
        exit(EXIT_FAILURE);
    }

    char sync_buf[BUFFER_SIZE] = {0};
    char command[BUFFER_SIZE] = {0};
    
    // send the SYNC_READY msg
    strcpy(sync_buf, "SYNC_READY\n");
    send(socket_fd, sync_buf, strlen(sync_buf), 0);

    // receive SYNC_START msg from leader
    memset(sync_buf, 0, BUFFER_SIZE);
    ssize_t bytes_received = recv(socket_fd, sync_buf, 11, 0);
    if (bytes_received <= 0 || strncmp(sync_buf, "SYNC_START\n", 11) != 0) {
        fprintf(stderr, "Failed to receive SYNC_START message from leader\n");
        fclose(temp_log);
        remove(temp_log_file);
        exit(EXIT_FAILURE);
    }

    printf("Received SYNC_START message from leader, starting syncing...\n");
    
    while (1) {
        memset(sync_buf, 0, BUFFER_SIZE);
        bytes_received = recv(socket_fd, sync_buf, BUFFER_SIZE, 0);
        if (bytes_received < 0) {
            perror("Error on leader-follower socket");
            fclose(temp_log);
            remove(temp_log_file);
            exit(EXIT_FAILURE);
        }

        ssize_t cmd_index = 0;
        for (size_t i = 0; i < strlen(sync_buf); i++) {
            command[cmd_index++] = sync_buf[i];
            if (sync_buf[i] == '\n') {
                if (strncmp(command, "SYNC_FINISHED", 13) == 0) {
                    printf("Received SYNC_FINISHED message from leader\n");
                    fclose(temp_log);
                    if (rename(temp_log_file, log_path) != 0) {
                        perror("Error replacing old log file with new one");
                        remove(temp_log_file);
                        exit(EXIT_FAILURE);
                    }

                    printf("Synced with leader, starting server...\n");
                    return;
                }

                command_response_t cmd_res = handle_command(command);
                if (cmd_res.type == PUT || cmd_res.type == DELETE) {
                    if (cmd_res.type == PUT) {
                        fprintf(temp_log, "PUT %s %s\n", cmd_res.data.key, cmd_res.data.value);
                        if (kv_put(cmd_res.data.key, cmd_res.data.value) != 0) {
                            fprintf(stderr, "Error executing PUT command during sync: key=%s, value=%s\n", cmd_res.data.key, cmd_res.data.value);
                        }
                    } else if (cmd_res.type == DELETE) {
                        fprintf(temp_log, "DELETE %s\n", cmd_res.data.key);
                        if (kv_delete(cmd_res.data.key) != 0) {
                            fprintf(stderr, "Error executing DELETE command during sync: key=%s\n", cmd_res.data.key);
                        }
                    }
                    fflush(temp_log);
                }
                cmd_index = 0;
                memset(command, 0, BUFFER_SIZE);
            }
        }
    }
}

int connect_with_leader_server(const char *leader_host, int leader_port, int follower_id) {
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
    start_follower_handshake(socket_fd, follower_id);

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
        
        printf("Received msg from leader: %s", buf);

        // Parse command first to validate
        command_response_t cmd_res = handle_command(buf);
        if (cmd_res.type == INVALID) {
            printf("Invalid command received from leader, ignoring: %s", buf);
            continue;
        }

        // Only write and execute valid PUT/DELETE commands
        switch (cmd_res.type) {
            case PUT:
                write_to_data_file(buf);
                if (kv_put(cmd_res.data.key, cmd_res.data.value) != 0) {
                    printf("Error executing PUT command from leader\n");
                }
                break;

            case DELETE:
                write_to_data_file(buf);
                if (kv_delete(cmd_res.data.key) != 0) {
                    printf("Error executing DELETE command from leader\n");
                }
                break;

            case GET:
            case CLOSE:
                break;

            default:
                printf("Unexpected command type from leader\n");
                break;
        }
    }
    return NULL;
}

void start_follower_server(int port, const char *leader_host, int leader_port, int id) {
    if (id == 1) {
        fprintf(stderr, "ID 1 is reserved for leader, please choose a different one\n");
        exit(EXIT_FAILURE);
    }

    pthread_t tid;

    mkdir(DATA_DIRECTORY, 0755);

    char log_path[128] = {0};
    snprintf(log_path, sizeof(log_path), "%s/follower_%d.log", DATA_DIRECTORY, id);
    int leader_socket_fd = connect_with_leader_server(leader_host, leader_port, id);
    pthread_create(&tid, NULL, handle_leader, (void *)(intptr_t) leader_socket_fd);
    pthread_detach(tid);

    start_tcp_server(port);
    close(leader_socket_fd);
}

static void start_leader_handshake(int socket_fd) {
    char handshake_res[128] = {0};
    strcpy(handshake_res, "HELLO LEADER\n");
    send(socket_fd, handshake_res, sizeof(handshake_res), 0);
    send_logs_to_follower(socket_fd);
}

static void start_follower_handshake(int socket_fd, int follower_id) {
    char handshake_buf[128] = {0};
    char log_path[128] = {0};

    printf("Initiating handshake with leader...\n");
    const char *follower_secret = get_shared_secret();
    snprintf(handshake_buf, sizeof(handshake_buf), "HELLO FOLLOWER|%d:%s\n", follower_id, follower_secret);
    send(socket_fd, handshake_buf, sizeof(handshake_buf), 0);

    memset(handshake_buf, 0, sizeof(handshake_buf));
    recv(socket_fd, handshake_buf, sizeof(handshake_buf), 0);
    if (strncmp("HELLO LEADER\n", handshake_buf, 13) != 0) {
        fprintf(stderr, "Failed to connect with leader shutting down. Reason: %s", handshake_buf);
        exit(EXIT_FAILURE);
    }

    printf("Leader handshake successful.\n");

    snprintf(log_path, sizeof(log_path), "%s/follower_%d.log", DATA_DIRECTORY, follower_id);

    sync_with_leader(socket_fd, log_path);

    replay_data_file(log_path);
    open_data_file(log_path);
}
