#ifndef LOG_H
#define LOG_H

#include "../core/datastore.h"

#define DATA_DIRECTORY "data"
#define LEADER_DATA_FILE "leader.log"
#define LEADER_TEMP_DATA_FILE "leader.tmp"
#define BUFFER_SIZE 1024
#define WRITE_THRESHOLD 100

void open_data_file(const char *path);

void replay_data_file(const char *path);

void write_to_data_file(const char *command);

#endif
