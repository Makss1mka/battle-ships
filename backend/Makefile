CC = gcc
CFLAGS = -W -Wall -Wextra -D_POSIX_C_SOURCE=200809L --std=c23
LDFLAGS = -lwebsockets -lmicrohttpd -ljansson -lpthread

SRC_DIR = src
BUILD_DIR = build
INCLUDE_DIR = $(SRC_DIR)/headers

SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC_FILES))

TARGET = $(BUILD_DIR)/main

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
		mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJ_FILES)
		$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
		$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c -o $@ $<

clean:
		rm -rf $(BUILD_DIR) $(TARGET)

rebuild: clean all

.PHONY: clean rebuild