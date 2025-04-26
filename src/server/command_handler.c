#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "command_handler.h"
#include "../core/datastore.h"

command_response_t handle_close(void) {
    command_response_t response = {0};
    response.type = CLOSE;
    return response;
}

command_response_t handle_put(const char *command) {
    command_response_t response = {0};
    response.type = PUT;
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
            strncpy(response.data.key, key, BUFFER_SIZE);
            strncpy(response.data.value, value, BUFFER_SIZE);
        } else {
            printf("Value missing for PUT command\n");
            response.type = INVALID;
        }
    } else {
        printf("Invalid command got: %s\n", command);
        response.type = INVALID;
    }
    return response;
}

command_response_t handle_get(const char *command) {
    command_response_t response = {0};
    response.type = GET;
    char buffer[BUFFER_SIZE];
    strncpy(buffer, command, BUFFER_SIZE);
    
    char *key;
    char *tokens = strtok(buffer, " \n");
    tokens = strtok(NULL, " \n");
    if (tokens) {
        key = tokens;
        strncpy(response.data.key, key, BUFFER_SIZE);
    } else {
        printf("Invalid command got: %s\n", command);
        response.type = INVALID;
    }
    return response;
}

command_response_t handle_delete(const char *command) {
    command_response_t response = {0};
    response.type = DELETE;
    char buffer[BUFFER_SIZE];
    strncpy(buffer, command, BUFFER_SIZE);
    
    char *key;
    char *tokens = strtok(buffer, " \n");
    tokens = strtok(NULL, " \n");
    if (tokens) {
        key = tokens;
        strncpy(response.data.key, key, BUFFER_SIZE);
    } else {
        printf("Invalid command got: %s\n", command);
        response.type = INVALID;
    }
    return response;
}

static command_response_t handle_invalid_command(void) {
    command_response_t response = {0};
    response.type = INVALID;
    printf("Invalid command\n");
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
