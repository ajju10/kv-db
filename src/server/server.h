#ifndef SERVER_H
#define SERVER_H

void *handle_client(void *arg);

void start_tcp_server(int port);

void start_leader_server(int port);

int connect_with_leader_server(const char *leader_host, int leader_port);

void start_follower_server(int port, const char *leader_host, int leader_port);

#endif
