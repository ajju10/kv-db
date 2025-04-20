#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdint.h>
#include "datastore.h"
#include "../transport/unix/unix_transport.h"

#define BUFFER_SIZE 256
#define WRITE_THRESHOLD 100
#define LOG_FILE "logs/datastore.log"
#define TEMP_LOG_FILE "logs/datastore.tmp"

FILE *fptr;
uint8_t write_counter;

void ensure_log_file_open() {
    if (fptr == NULL) {
        fptr = fopen(LOG_FILE, "a");
        if (fptr == NULL) {
            perror("Failed to open log file");
            exit(EXIT_FAILURE);
        }
        setvbuf(fptr, NULL, _IONBF, 0);  // Disable buffering
    }
}

void compact_log_file() {
    // Close the current log file
    if (fptr != NULL) {
        fclose(fptr);
        fptr = NULL;
    }

    FILE *temp_log = fopen(TEMP_LOG_FILE, "w");
    if (temp_log == NULL) {
        perror("Failed to open temp log file");
        ensure_log_file_open();  // Reopen the log file
        return;
    }

    struct key_value *entry, *tmp;
    HASH_ITER(hh, store, entry, tmp) {
        fprintf(temp_log, "PUT %s %s\n", entry->key, entry->value);
    }

    fflush(temp_log);
    fclose(temp_log);

    if (rename(TEMP_LOG_FILE, LOG_FILE) != 0) {
        perror("Failed to replace log file");
        return;
    }

    write_counter = 0;
}

void handle_close(int client_fd, char *send_buf) {
    strncpy(send_buf, "BYE\n", BUFFER_SIZE);
    write(client_fd, send_buf, BUFFER_SIZE);
    close(client_fd);
    printf("Socket closed\n");
}

void handle_put(char *buffer, char *send_buf, int client_fd) {
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

            strncpy(send_buf, "OK\n", BUFFER_SIZE);
        } else {
            printf("Value missing for PUT command\n");
            strncpy(send_buf, "failure\n", BUFFER_SIZE);
        }
    } else {
        printf("Key missing for PUT command\n");
        strncpy(send_buf, "invalid command expected PUT <key> <value>\n", BUFFER_SIZE);
    }
    write(client_fd, send_buf, BUFFER_SIZE);
}

void handle_get(char *buffer, char *send_buf, int client_fd) {
    char *key;
    char *tokens = strtok(buffer, " ");

    tokens = strtok(NULL, " ");
    if (tokens) {
        key = tokens;
        char *value = get_key(key);
        if (value == NULL) {
            strncpy(send_buf, "\n", BUFFER_SIZE);  // Empty line for non-existent key
        } else {
            snprintf(send_buf, BUFFER_SIZE, "%s\n", value);  // Value followed by newline
        }
    } else {
        printf("No key found for GET\n");
        strncpy(send_buf, "invalid command expected GET <key>\n", BUFFER_SIZE);
    }
    write(client_fd, send_buf, BUFFER_SIZE);
}

void handle_delete(char *buffer, char *send_buf, int client_fd) {
    char *key;
    char *tokens = strtok(buffer, " ");
    tokens = strtok(NULL, " ");
    if (tokens) {
        key = tokens;
        char *value = get_key(key);
        
        if (value == NULL) {
            strncpy(send_buf, "NOT_FOUND\n", BUFFER_SIZE);
        } else {
            ensure_log_file_open();
            fprintf(fptr, "DELETE %s\n", key);
            fflush(fptr);  // Ensure data is written to disk
            purge_key(key);
            strncpy(send_buf, "OK\n", BUFFER_SIZE);
        }
    } else {
        printf("Key missing for DELETE command\n");
        strncpy(send_buf, "invalid command expected DELETE <key>\n", BUFFER_SIZE);
    }
    write(client_fd, send_buf, BUFFER_SIZE);
}

void handle_invalid_command(char *send_buf, int client_fd) {
    printf("Invalid command\n");
    strncpy(send_buf, "Invalid command\n", BUFFER_SIZE);
    write(client_fd, send_buf, BUFFER_SIZE);
}

void restore_from_log() {
    FILE *log_file = fopen(LOG_FILE, "r");
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
    printf("Commands:\n");
    printf("  PUT <key> <value> - Store a key-value pair\n");
    printf("  GET <key> - Retrieve the value for a key\n");
    printf("  DELETE <key> - Remove a key-value pair\n");
    printf("  CLOSE - Close the connection\n");

    unix_init();

    char buffer[BUFFER_SIZE];
    char send_buf[BUFFER_SIZE];

    write_counter = 0;

    fptr = fopen(LOG_FILE, "a");
    if (fptr == NULL) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }
    setvbuf(fptr, NULL, _IONBF, 0);
    
    memset(&buffer, 0, BUFFER_SIZE);
    memset(&send_buf, 0, BUFFER_SIZE);

    restore_from_log();

    while (1) {
        int client_fd = unix_accept();
        if (client_fd < 0) {
            continue;
        }

        while (1) {
            int ret = read(client_fd, buffer, BUFFER_SIZE);
            if (ret == -1) {
                perror("Read failure");
                exit(EXIT_FAILURE);
            }

            if (ret == 0 || strncmp(buffer, "CLOSE", 5) == 0) {
                handle_close(client_fd, send_buf);
                break;
            }

            if (strncmp(buffer, "PUT", 3) == 0) {
                handle_put(buffer, send_buf, client_fd);
                write_counter++;
            } else if (strncmp(buffer, "GET", 3) == 0) {
                handle_get(buffer, send_buf, client_fd);
            } else if (strncmp(buffer, "DELETE", 6) == 0) {
                handle_delete(buffer, send_buf, client_fd);
                write_counter++;
            } else {
                handle_invalid_command(send_buf, client_fd);
            }

            memset(buffer, 0, BUFFER_SIZE); 
            memset(send_buf, 0, BUFFER_SIZE);

            if (write_counter >= WRITE_THRESHOLD)
                compact_log_file();
        }

        close(client_fd);
    }

    unix_close();
    fclose(fptr);
    return 0;
}
