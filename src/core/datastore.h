#ifndef DATASTORE_H
#define DATASTORE_H

#include "../../lib/uthash.h"

typedef struct db_entry {
    char *key;
    char *value;
    UT_hash_handle hh;
} db_entry_t;

void kv_init();

void kv_shutdown();

char *kv_get(const char *key);

int kv_put(const char *key, const char *value);

int kv_delete(const char *key);

void open_log_file();

void kv_compact();

void replay_log();

void write_log(const char *command);

#endif
