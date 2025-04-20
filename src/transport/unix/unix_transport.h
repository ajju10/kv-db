#ifndef UNIX_TRANSPORT_H
#define UNIX_TRANSPORT_H

#define SOCKET_PATH "/tmp/keyval.sock"
#define BUFFER_SIZE 256

void unix_init();

int unix_accept();

void unix_close();

#endif
