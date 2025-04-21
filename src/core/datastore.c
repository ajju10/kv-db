#include <pthread.h>
#include <stdio.h>
#include "datastore.h"
#include "../transport/unix/unix_transport.h"

#define BUFFER_SIZE 256
#define WRITE_THRESHOLD 100
#define LOG_FILE "logs/datastore.log"
#define TEMP_LOG_FILE "logs/datastore.tmp"

db_entry_t *store = NULL;

pthread_mutex_t store_mutex = PTHREAD_MUTEX_INITIALIZER;

FILE *fptr;
int write_counter;

void kv_init() {
    write_counter = 0;
    replay_log();
    open_log_file();
}

void kv_shutdown() {
    fclose(fptr);
}

char *kv_get(const char *key) {
    pthread_mutex_lock(&store_mutex);
    db_entry_t *entry;
    HASH_FIND_STR(store, key, entry);
    pthread_mutex_unlock(&store_mutex);
    return entry ? entry->value : "";
}

int kv_put(const char *key, const char *value) {
    pthread_mutex_lock(&store_mutex);  
    char log_buffer[BUFFER_SIZE];
    db_entry_t *entry;
    
    HASH_FIND_STR(store, key, entry);
    if (entry) {
        sprintf(log_buffer, "PUT %s %s\n", key, value);
        write_log(log_buffer);
        free(entry->value);
        entry->value = strdup(value);
        pthread_mutex_unlock(&store_mutex);
        return entry->value ? 0 : -1;
    }

    entry = malloc(sizeof(db_entry_t));
    if (!entry) return -1;

    sprintf(log_buffer, "PUT %s %s\n", key, value);
    write_log(log_buffer);
    entry->key = strdup(key);
    entry->value = strdup(value);
    HASH_ADD_KEYPTR(hh, store, entry->key, strlen(entry->key), entry);
    pthread_mutex_unlock(&store_mutex);
    return 0;
}

int kv_delete(const char *key) {
    pthread_mutex_lock(&store_mutex);
    char log_buffer[BUFFER_SIZE];
    db_entry_t *entry;

    HASH_FIND_STR(store, key, entry);
    if (entry) {
        sprintf(log_buffer, "DELETE %s\n", key);
        write_log(log_buffer);
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

void open_log_file() {
    if (fptr == NULL) {
        fptr = fopen(LOG_FILE, "a");
        if (fptr == NULL) {
            perror("Failed to open log file");
            exit(EXIT_FAILURE);
        }
        setvbuf(fptr, NULL, _IONBF, 0);
    }
}

void kv_compact() {
    if (fptr != NULL) {
        fclose(fptr);
        fptr = NULL;
    }

    FILE *temp_log = fopen(TEMP_LOG_FILE, "w");
    if (temp_log == NULL) {
        perror("Failed to open temp log file");
        return;
    }

    db_entry_t *entry, *tmp;
    HASH_ITER(hh, store, entry, tmp) {
        fprintf(temp_log, "PUT %s %s\n", entry->key, entry->value);
    }

    fflush(temp_log);
    fclose(temp_log);

    if (rename(TEMP_LOG_FILE, LOG_FILE) != 0) {
        perror("Failed to replace log file");
        return;
    }

    write_counter = 0;
}

void replay_log() {
    FILE *log_file = fopen(LOG_FILE, "r");
    if (log_file == NULL) {
        return;
    }

    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), log_file)) {        
        char command[10];
        char key[BUFFER_SIZE];
        char value[BUFFER_SIZE];
        line[strcspn(line, "\n")] = 0;
        if (sscanf(line, "%s %s %[^\n]", command, key, value) >= 2) {
            if (strcmp(command, "PUT") == 0) {
                kv_put(key, value);
            } else if (strcmp(command, "DELETE") == 0) {
                kv_delete(key);
            }
        }
    }
    fclose(log_file);
    printf("State restored from log file\n");
}

void write_log(const char *command) {
    open_log_file();
    fprintf(fptr, "%s", command);
    fflush(fptr);
    write_counter++;
    if (write_counter >= WRITE_THRESHOLD)
        kv_compact();
}

