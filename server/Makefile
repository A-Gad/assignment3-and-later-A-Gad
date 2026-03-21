CC ?= $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Wextra -Werror -std=c99
TARGET = aesdsocket

.PHONY: all default clean

all default: $(TARGET)

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(TARGET)