#ifndef DATASTORE_H
#define DATASTORE_H

#include "../lib/uthash.h"

struct key_value {
    char *key;
    char *value;
    UT_hash_handle hh;
};

// Declare the store variable as extern to avoid multiple definitions
extern struct key_value *store;

char* get_key(char* key);

void set_key_value(char* key, char* value);

void purge_key(char* key);

#endif
