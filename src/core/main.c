#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdint.h>
#include "datastore.h"
#include "../transport/unix/unix_transport.h"


void handle_close(int client_fd, char *send_buf) {
    memset(send_buf, 0, BUFFER_SIZE);
    strncpy(send_buf, "BYE\n", BUFFER_SIZE);
    write(client_fd, send_buf, BUFFER_SIZE);
    close(client_fd);
    printf("Client %d disconnected\n", client_fd);
}

void handle_put(char *buffer, char *send_buf, int client_fd) {
    char *key, *value;
    char *tokens = strtok(buffer, " \n");
    tokens = strtok(NULL, " \n");
    if (tokens) {
        key = tokens;
        tokens = strtok(NULL, "\n");
        if (tokens) {
            value = tokens;
            if (kv_put(key, value) == 0) {
                strncpy(send_buf, "OK\n", BUFFER_SIZE);
            } else {
                strncpy(send_buf, "ERROR\n", BUFFER_SIZE);
            }
        } else {
            printf("Value missing for PUT command\n");
            snprintf(send_buf, BUFFER_SIZE, "Missing value for key: %s\n", key);
        }
    } else {
        printf("Invalid command got: %s\n", buffer);
        strncpy(send_buf, "Invalid command expected PUT <key> <value>\n", BUFFER_SIZE);
    }
    write(client_fd, send_buf, BUFFER_SIZE);
}

void handle_get(char *buffer, char *send_buf, int client_fd) {
    char *key;
    char *tokens = strtok(buffer, " \n");
    tokens = strtok(NULL, " \n");
    if (tokens) {
        key = tokens;
        char *value = kv_get(key);
        if (value[0] == '\0') {
            strncpy(send_buf, "\n", BUFFER_SIZE);
        } else {
            snprintf(send_buf, BUFFER_SIZE, "%s\n", value);
        }
    } else {
        printf("Invalid command got: %s\n", buffer);
        strncpy(send_buf, "Invalid command expected GET <key>\n", BUFFER_SIZE);
    }
    write(client_fd, send_buf, BUFFER_SIZE);
}

void handle_delete(char *buffer, char *send_buf, int client_fd) {
    char *key;
    char *tokens = strtok(buffer, " \n");
    tokens = strtok(NULL, " \n");
    if (tokens) {
        key = tokens;
        if (kv_delete(key) == 1) {
            strncpy(send_buf, "NOT_FOUND\n", BUFFER_SIZE);
        } else {
            strncpy(send_buf, "OK\n", BUFFER_SIZE);
        }
    } else {
        printf("Invalid command got: %s\n", buffer);
        strncpy(send_buf, "Invalid command expected DELETE <key>\n", BUFFER_SIZE);
    }
    write(client_fd, send_buf, BUFFER_SIZE);
}

void handle_invalid_command(char *send_buf, int client_fd) {
    printf("Invalid command\n");
    strncpy(send_buf, "Invalid command\n", BUFFER_SIZE);
    write(client_fd, send_buf, BUFFER_SIZE);
}

void handle_client(int client_fd) {
    char read_buf[BUFFER_SIZE];
    char write_buf[BUFFER_SIZE];

    memset(&read_buf, 0, BUFFER_SIZE);
    memset(&write_buf, 0, BUFFER_SIZE);

    while (1) {
        int ret = read(client_fd, read_buf, BUFFER_SIZE);
        if (ret < 0) {
            perror("Read failed from client\n");
            close(client_fd);
            return;
        }

        if (strncmp(read_buf, "CLOSE", 5) == 0) {
            handle_close(client_fd, write_buf);
            break;
        } else if (strncmp(read_buf, "PUT", 3) == 0) {
            handle_put(read_buf, write_buf, client_fd);
        } else if (strncmp(read_buf, "GET", 3) == 0) {
            handle_get(read_buf, write_buf, client_fd);
        } else if (strncmp(read_buf, "DELETE", 6) == 0) {
            handle_delete(read_buf, write_buf, client_fd);
        } else {
            handle_invalid_command(write_buf, client_fd);
        }
    }

    close(client_fd);
}

int main() {
    printf("Welcome to the Key-Value Store!\n");
    printf("Commands:\n");
    printf("  PUT <key> <value> - Store a key-value pair\n");
    printf("  GET <key> - Retrieve the value for a key\n");
    printf("  DELETE <key> - Remove a key-value pair\n");
    printf("  CLOSE - Close the connection\n");

    kv_init();

    char buffer[BUFFER_SIZE];
    char send_buf[BUFFER_SIZE];

    memset(&buffer, 0, BUFFER_SIZE);
    memset(&send_buf, 0, BUFFER_SIZE);

    while (1) {
        int client_fd = unix_accept();
        if (client_fd < 0) {
            continue;
        }

        handle_client(client_fd);
    }

    kv_shutdown();

    return 0;
}
