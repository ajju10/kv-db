#include <stdio.h>
#include <stdlib.h>
#include "../server/server.h"
#include "datastore.h"

void parse_args(int argc, char *argv[], config_t *config) {
    strcpy(config->role, "leader");
    config->id = 1;
    config->port = 5000;
    strcpy(config->leader_host, "127.0.0.1");
    config->leader_port = 5000;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) {
            strncpy(config->role, argv[++i], sizeof(config->role) - 1);
        } else if (strcmp(argv[i], "--id") == 0 && i + 1 < argc) {
            config->id = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            config->port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--leader_host") == 0 && i + 1 < argc) {
            strncpy(config->leader_host, argv[++i], sizeof(config->leader_host) - 1);
        } else if (strcmp(argv[i], "--leader_port") == 0 && i + 1 < argc) {
            config->leader_port = atoi(argv[++i]);
        }
    }
}

int main(int argc, char *argv[]) {
    printf("Welcome to the Key-Value Store!\n");
    printf("Commands:\n");
    printf("  PUT <key> <value> - Store a key-value pair\n");
    printf("  GET <key> - Retrieve the value for a key\n");
    printf("  DELETE <key> - Remove a key-value pair\n");
    printf("  PING - Test server connection\n");
    printf("  CLOSE - Close the connection\n");

    config_t config = {0};
    parse_args(argc, argv, &config);

    set_server_config((const config_t *) &config);
    if (strcmp(config.role, "leader") == 0) {
        printf("Starting as leader on port %d\n", config.port);
        start_leader_server(config.port);
    } else {
        printf("Starting as follower on port %d, connecting to leader at %s:%d\n", config.port, config.leader_host, config.leader_port);
        start_follower_server(config.port, config.leader_host, config.leader_port, config.id);
    }

    return 0;
}
