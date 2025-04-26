#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../lib/uthash.h"
#include "../core/datastore.h"
#include "log.h"

#define FILE_PATH_SIZE 128

FILE *file;
int write_counter = 0;
static char current_log_path[FILE_PATH_SIZE];

void open_data_file(const char *path) {
    if (file == NULL) {
        strncpy(current_log_path, path, FILE_PATH_SIZE - 1);
        current_log_path[FILE_PATH_SIZE - 1] = '\0';
        file = fopen(path, "a");
        if (file == NULL) {
            perror("Failed to open log file");
            exit(EXIT_FAILURE);
        }
        setvbuf(file, NULL, _IONBF, 0);
    }
}

void replay_data_file(const char *path) {
    FILE *log_file = fopen(path, "r");
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

static void compact_data_file(const char *filename) {
    if (file != NULL) {
        fclose(file);
        file = NULL;
    }

    // Create temporary filename by appending .tmp to original filename
    char temp_filename[FILE_PATH_SIZE];
    snprintf(temp_filename, sizeof(temp_filename), "%s.tmp", filename);

    FILE *temp_log = fopen(temp_filename, "w");
    if (temp_log == NULL) {
        perror("Failed to open temp log file");
        open_data_file(filename);
        return;
    }

    // Write current state to temp file
    db_entry_t *entry, *tmp;
    HASH_ITER(hh, store, entry, tmp) {
        if (fprintf(temp_log, "PUT %s %s\n", entry->key, entry->value) < 0) {
            perror("Failed to write to temp log file");
            fclose(temp_log);
            remove(temp_filename);
            open_data_file(filename);
            return;
        }
    }

    if (fflush(temp_log) != 0) {
        perror("Failed to flush temp log file");
        fclose(temp_log);
        remove(temp_filename);
        open_data_file(filename);
        return;
    }

    fclose(temp_log);

    // Atomically replace the old file with the new one
    if (rename(temp_filename, filename) != 0) {
        perror("Failed to replace log file");
        remove(temp_filename);
        open_data_file(filename);
        return;
    }

    // Reset write counter and reopen the file for new writes
    write_counter = 0;
    open_data_file(filename);
}

void write_to_data_file(const char *command) {
    fprintf(file, "%s", command);
    fflush(file);
    write_counter++;
    if (write_counter >= WRITE_THRESHOLD) {
        compact_data_file(current_log_path);
    }
}
