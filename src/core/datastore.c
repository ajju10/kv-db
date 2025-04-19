#include "datastore.h"

struct key_value *store = NULL;

char* get_key(char* key) {
    struct key_value *kv;
    HASH_FIND_STR(store, key, kv);
    return kv ? kv->value : NULL;
}

void set_key_value(char* key, char* value) {
    struct key_value *data;
    data = malloc(sizeof(*data));
    data->key = strdup(key);
    data->value = strdup(value);
    HASH_ADD_KEYPTR(hh, store, data->key, strlen(key), data);
}

void purge_key(char* key) {
    struct key_value *kv;
    HASH_FIND_STR(store, key, kv);
    if (kv) {
        HASH_DEL(store, kv);
        free(kv->key);
        free(kv->value);
        free(kv);
    }
}
