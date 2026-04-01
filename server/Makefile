CC ?= $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Wextra -Werror -std=c99
TARGET = aesdsocket
LDFLAGS = -lrt -lpthread
.PHONY: all default clean

all default: $(TARGET)

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET)