#include <pthread.h>
#include <stdio.h>
#include "datastore.h"

db_entry_t *store = NULL;
pthread_mutex_t store_mutex = PTHREAD_MUTEX_INITIALIZER;

char *kv_get(const char *key) {
    pthread_mutex_lock(&store_mutex);
    db_entry_t *entry;
    HASH_FIND_STR(store, key, entry);
    pthread_mutex_unlock(&store_mutex);
    return entry ? entry->value : "";
}

int kv_put(const char *key, const char *value) {
    pthread_mutex_lock(&store_mutex);  
    db_entry_t *entry;
    
    HASH_FIND_STR(store, key, entry);
    if (entry) {
        free(entry->value);
        entry->value = strdup(value);
        pthread_mutex_unlock(&store_mutex);
        return entry->value ? 0 : -1;
    }

    entry = malloc(sizeof(db_entry_t));
    if (!entry) {
        pthread_mutex_unlock(&store_mutex);
        return -1;
    }

    entry->key = strdup(key);
    entry->value = strdup(value);
    if (!entry->key || !entry->value) {
        free(entry->key);
        free(entry->value);
        free(entry);
        pthread_mutex_unlock(&store_mutex);
        return -1;
    }
    
    HASH_ADD_KEYPTR(hh, store, entry->key, strlen(entry->key), entry);
    pthread_mutex_unlock(&store_mutex);
    return 0;
}

int kv_delete(const char *key) {
    pthread_mutex_lock(&store_mutex);
    db_entry_t *entry;

    HASH_FIND_STR(store, key, entry);
    if (entry) {
        HASH_DEL(store, entry);
        free(entry->key);
        free(entry->value);
        free(entry);
        pthread_mutex_unlock(&store_mutex);
        return 0;
    }

    pthread_mutex_unlock(&store_mutex);
    return 1;
}

