CC = gcc
CFLAGS = -O2 -Wall -Wextra -Iinclude `pkg-config --cflags gtk4`
LIBS = `pkg-config --libs gtk4` -lm

TARGET = vresources
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

SRCS = $(addprefix $(SRC_DIR)/, main.c system_reader.c charts.c ui.c process_reader.c process_view.c process_actions.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

.PHONY: all clean
