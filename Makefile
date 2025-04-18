CC = gcc
CFLAGS = -Iinclude -Wall -Wextra
SRC = src/main.c src/datastore.c
OBJ = $(SRC:src/%.c=build/%.o)
TARGET = build/key_value_store

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $^
	rm -f $(OBJ)

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean run