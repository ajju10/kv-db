#ifndef DATASTORE_H
#define DATASTORE_H

#include "../../lib/uthash.h"

typedef struct db_entry {
    char *key;
    char *value;
    UT_hash_handle hh;
} db_entry_t;

extern db_entry_t *store;

char *kv_get(const char *key);

int kv_put(const char *key, const char *value);

int kv_delete(const char *key);

#endif
