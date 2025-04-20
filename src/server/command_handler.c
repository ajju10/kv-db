#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "command_handler.h"
#include "../core/datastore.h"

command_response_t handle_close(void) {
    command_response_t response = {0};
    strncpy(response.response, "BYE\n", BUFFER_SIZE);
    response.should_close = 1;
    return response;
}

command_response_t handle_put(const char *command) {
    command_response_t response = {0};
    char buffer[BUFFER_SIZE];
    strncpy(buffer, command, BUFFER_SIZE);
    
    char *key, *value;
    char *tokens = strtok(buffer, " \n");
    tokens = strtok(NULL, " \n");
    if (tokens) {
        key = tokens;
        tokens = strtok(NULL, "\n");
        if (tokens) {
            value = tokens;
            if (kv_put(key, value) == 0) {
                strncpy(response.response, "OK\n", BUFFER_SIZE);
            } else {
                strncpy(response.response, "ERROR\n", BUFFER_SIZE);
            }
        } else {
            printf("Value missing for PUT command\n");
            snprintf(response.response, BUFFER_SIZE, "Missing value for key: %s\n", key);
        }
    } else {
        printf("Invalid command got: %s\n", command);
        strncpy(response.response, "Invalid command expected PUT <key> <value>\n", BUFFER_SIZE);
    }
    return response;
}

command_response_t handle_get(const char *command) {
    command_response_t response = {0};
    char buffer[BUFFER_SIZE];
    strncpy(buffer, command, BUFFER_SIZE);
    
    char *key;
    char *tokens = strtok(buffer, " \n");
    tokens = strtok(NULL, " \n");
    if (tokens) {
        key = tokens;
        char *value = kv_get(key);
        if (value[0] == '\0') {
            strncpy(response.response, "\n", BUFFER_SIZE);
        } else {
            snprintf(response.response, BUFFER_SIZE, "%s\n", value);
        }
    } else {
        printf("Invalid command got: %s\n", command);
        strncpy(response.response, "Invalid command expected GET <key>\n", BUFFER_SIZE);
    }
    return response;
}

command_response_t handle_delete(const char *command) {
    command_response_t response = {0};
    char buffer[BUFFER_SIZE];
    strncpy(buffer, command, BUFFER_SIZE);
    
    char *key;
    char *tokens = strtok(buffer, " \n");
    tokens = strtok(NULL, " \n");
    if (tokens) {
        key = tokens;
        if (kv_delete(key) == 1) {
            strncpy(response.response, "NOT_FOUND\n", BUFFER_SIZE);
        } else {
            strncpy(response.response, "OK\n", BUFFER_SIZE);
        }
    } else {
        printf("Invalid command got: %s\n", command);
        strncpy(response.response, "Invalid command expected DELETE <key>\n", BUFFER_SIZE);
    }
    return response;
}

static command_response_t handle_invalid_command(void) {
    command_response_t response = {0};
    printf("Invalid command\n");
    strncpy(response.response, "Invalid command\n", BUFFER_SIZE);
    return response;
}

command_response_t handle_command(const char *command) {
    if (strncmp(command, "CLOSE", 5) == 0) {
        return handle_close();
    } else if (strncmp(command, "PUT", 3) == 0) {
        return handle_put(command);
    } else if (strncmp(command, "GET", 3) == 0) {
        return handle_get(command);
    } else if (strncmp(command, "DELETE", 6) == 0) {
        return handle_delete(command);
    }
    return handle_invalid_command();
}
