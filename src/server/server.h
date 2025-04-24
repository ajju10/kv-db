#ifndef SERVER_H
#define SERVER_H

typedef struct config {
    char role[10];
    int port;
    char leader_host[64];
    int leader_port;
} config_t;

void set_server_config(const config_t* config);

void *handle_client(void *arg);

void start_leader_server(int port);

int connect_with_leader_server(const char *leader_host, int leader_port);

void start_follower_server(int port, const char *leader_host, int leader_port);

#endif
