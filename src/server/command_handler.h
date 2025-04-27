#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#define BUFFER_SIZE 1024

typedef enum command_type {
    GET,
    PUT,
    DELETE,
    CLOSE,
    PING,
    INVALID
} command_type_t;

typedef struct command_data {
    char key[BUFFER_SIZE];
    char value[BUFFER_SIZE];
} command_data_t;

typedef struct command_response {
    command_type_t type;
    command_data_t data;
} command_response_t;

command_response_t handle_command(const char *command);

command_response_t handle_close(void);

command_response_t handle_put(const char *command);

command_response_t handle_get(const char *command);

command_response_t handle_delete(const char *command);

#endif
