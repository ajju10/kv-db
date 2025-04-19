#define SOCKET_NAME "/tmp/keyval.sock"
#define BUFFER_SIZE 256

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../include/datastore.h"

FILE *fptr;

void ensure_log_file_open() {
    if (fptr == NULL) {
        fptr = fopen("datastore.log", "a");
        if (fptr == NULL) {
            perror("Failed to open log file");
            exit(EXIT_FAILURE);
        }
        setvbuf(fptr, NULL, _IONBF, 0);  // Disable buffering
    }
}

void handle_close(int data_socket, char *send_buf) {
    strncpy(send_buf, "Closing connection...", BUFFER_SIZE);
    write(data_socket, send_buf, BUFFER_SIZE);
    close(data_socket);
    printf("Socket closed\n");
}

void handle_put(char *buffer, char *send_buf, int data_socket) {
    char *key, *value;
    char *tokens = strtok(buffer, " ");
    tokens = strtok(NULL, " ");
    if (tokens) {
        key = tokens;
        tokens = strtok(NULL, " ");
        if (tokens) {
            value = tokens;
            printf("Key: %s, Value: %s\n", key, value);

            ensure_log_file_open();
            fprintf(fptr, "PUT %s %s\n", key, value);
            fflush(fptr);  // Ensure data is written to disk
            set_key_value(key, value);

            strncpy(send_buf, "success", BUFFER_SIZE);
        } else {
            printf("Value missing for PUT command\n");
            strncpy(send_buf, "failure", BUFFER_SIZE);
        }
    } else {
        printf("Key missing for PUT command\n");
        strncpy(send_buf, "invalid command expected PUT <key> <value>", BUFFER_SIZE);
    }
    write(data_socket, send_buf, BUFFER_SIZE);
}

void handle_get(char *buffer, char *send_buf, int data_socket) {
    char *key;
    char *tokens = strtok(buffer, " ");

    tokens = strtok(NULL, " ");
    if (tokens) {
        key = tokens;
        char *value = get_key(key);
        if (value == NULL) {
            strncpy(send_buf, "Key not found", BUFFER_SIZE);
        } else {
            strncpy(send_buf, value, BUFFER_SIZE);
        }
    } else {
        printf("No key found for GET\n");
        strncpy(send_buf, "invalid command expected GET <key>", BUFFER_SIZE);
    }
    write(data_socket, send_buf, BUFFER_SIZE);
}

void handle_delete(char *buffer, char *send_buf, int data_socket) {
    char *key;
    char *tokens = strtok(buffer, " ");
    tokens = strtok(NULL, " ");
    if (tokens) {
        key = tokens;

        ensure_log_file_open();
        fprintf(fptr, "DELETE %s\n", key);
        fflush(fptr);  // Ensure data is written to disk
        purge_key(key);

        strncpy(send_buf, "success", BUFFER_SIZE);
    } else {
        printf("Key missing for DELETE command\n");
        strncpy(send_buf, "invalid command expected DELETE <key>", BUFFER_SIZE);
    }
    write(data_socket, send_buf, BUFFER_SIZE);
}

void handle_invalid_command(char *send_buf, int data_socket) {
    printf("Invalid command\n");
    strncpy(send_buf, "Invalid command\n", BUFFER_SIZE);
    write(data_socket, send_buf, BUFFER_SIZE);
}

void restore_from_log() {
    FILE *log_file = fopen("datastore.log", "r");
    if (log_file == NULL) {
        // If file doesn't exist, just return as this might be first run
        return;
    }

    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), log_file)) {
        // Remove newline if present
        line[strcspn(line, "\n")] = 0;
        
        char command[10];
        char key[BUFFER_SIZE];
        char value[BUFFER_SIZE];

        if (sscanf(line, "%s %s %[^\n]", command, key, value) >= 2) {
            if (strcmp(command, "PUT") == 0) {
                set_key_value(key, value);
            } else if (strcmp(command, "DELETE") == 0) {
                purge_key(key);
            }
        }
    }
    fclose(log_file);
    printf("State restored from log file\n");
}

int main() {
    printf("Welcome to the Key-Value Store!\n");
    printf("Server is starting...\n");
    printf("Listening on socket: %s\n", SOCKET_NAME);
    printf("Commands:\n");
    printf("  PUT <key> <value> - Store a key-value pair\n");
    printf("  GET <key> - Retrieve the value for a key\n");
    printf("  DELETE <key> - Remove a key-value pair\n");
    printf("  CLOSE - Close the connection\n");
    struct sockaddr_un addr;
    int ret;
    int listen_socket;
    int data_socket;
    char buffer[BUFFER_SIZE];
    char send_buf[BUFFER_SIZE];

    unlink(SOCKET_NAME);

    listen_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_socket == -1) {
        perror("Socket initialization failed");
        exit(EXIT_FAILURE);
    }

    fptr = fopen("datastore.log", "a");
    if (fptr == NULL) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }
    setvbuf(fptr, NULL, _IONBF, 0);

    memset(&addr, 0, sizeof(struct sockaddr_un));
    memset(&buffer, 0, BUFFER_SIZE);
    memset(&send_buf, 0, BUFFER_SIZE);

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_NAME, sizeof(addr.sun_path) - 1);

    ret = bind(listen_socket, (const struct sockaddr*) &addr, sizeof(struct sockaddr_un));
    if (ret == -1) {
        perror("Error while binding");
        exit(EXIT_FAILURE);
    }

    ret = listen(listen_socket, 20);
    if (ret == -1) {
        perror("Listen error");
        exit(EXIT_FAILURE);
    }

    restore_from_log();

    while (1) {
        data_socket = accept(listen_socket, NULL, NULL);
        if (data_socket == -1) {
            perror("Accept failure");
            exit(EXIT_FAILURE);
        }

        while (1) {
            ret = read(data_socket, buffer, BUFFER_SIZE);
            if (ret == -1) {
                perror("Read failure");
                exit(EXIT_FAILURE);
            }

            if (ret == 0 || strncmp(buffer, "CLOSE", 5) == 0) {
                handle_close(data_socket, send_buf);
                break;
            }

            if (strncmp(buffer, "PUT", 3) == 0) {
                handle_put(buffer, send_buf, data_socket);
            } else if (strncmp(buffer, "GET", 3) == 0) {
                handle_get(buffer, send_buf, data_socket);
            } else if (strncmp(buffer, "DELETE", 6) == 0) {
                handle_delete(buffer, send_buf, data_socket);
            } else {
                handle_invalid_command(send_buf, data_socket);
            }

            memset(buffer, 0, BUFFER_SIZE); 
            memset(send_buf, 0, BUFFER_SIZE);
        }

        close(data_socket);
    }

    close(listen_socket);
    fclose(fptr);
    unlink(SOCKET_NAME);
    printf("Socket unlinked\n");
    printf("Server shutting down...\n");

    return 0;
}
