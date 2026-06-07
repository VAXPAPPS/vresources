CC = gcc
CFLAGS = -O2 -Wall -Wextra `pkg-config --cflags gtk4`
LIBS = `pkg-config --libs gtk4` -lm

TARGET = vresources
SRCS = main.c system_reader.c charts.c ui.c process_reader.c process_view.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
