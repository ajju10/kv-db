#ifndef DATASTORE_H
#define DATASTORE_H

#include "../../lib/uthash.h"

struct key_value {
    char *key;
    char *value;
    UT_hash_handle hh;
};

extern struct key_value *store;

void kv_init();

void kv_shutdown();

char *kv_get(const char *key);

int kv_put(const char *key, const char *value);

int kv_delete(const char *key);

void open_log_file();

void kv_compact();

void replay_log();

void write_log(const char *command);

char* get_key(char* key);

void set_key_value(char* key, char* value);

void purge_key(char* key);

#endif
