#include <stdio.h>
#include <stdlib.h>
#include "../server/server.h"
#include "datastore.h"

int main() {
    printf("Welcome to the Key-Value Store!\n");
    printf("Commands:\n");
    printf("  PUT <key> <value> - Store a key-value pair\n");
    printf("  GET <key> - Retrieve the value for a key\n");
    printf("  DELETE <key> - Remove a key-value pair\n");
    printf("  CLOSE - Close the connection\n");

    kv_init();
    start_tcp_server();
    kv_shutdown();

    return 0;
}
