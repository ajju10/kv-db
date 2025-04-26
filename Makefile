CC = gcc
CFLAGS = -Wall -Wextra -I$(SRC_DIR)/core -I. -g

BUILD_DIR = build
SRC_DIR = src

# Source files
CORE_SRC = $(wildcard $(SRC_DIR)/core/*.c)
SERVER_SRC = $(wildcard $(SRC_DIR)/server/*.c)
LOG_SRC = $(wildcard $(SRC_DIR)/logs/*.c)
ALL_SRC = $(CORE_SRC) $(SERVER_SRC) $(LOG_SRC)

# Object files
OBJ = $(ALL_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Binary name
TARGET = $(BUILD_DIR)/keyval

.PHONY: all clean run dirs

all: dirs $(TARGET)

dirs:
	@mkdir -p $(BUILD_DIR)/core $(BUILD_DIR)/server $(BUILD_DIR)/logs

$(TARGET): $(OBJ)
	$(CC) -o $@ $^

# Pattern rule for object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

# Clean only build artifacts
clean:
	rm -rf $(BUILD_DIR)/*
	@echo "Cleaned build artifacts"
