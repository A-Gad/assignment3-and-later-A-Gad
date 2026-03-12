CC=$(CROSS_COMPILE)gcc
TARGET=writer

all: $(TARGET)

$(TARGET) : writer.c
	$(CC) writer.c -o $(TARGET)

clean:
	rm -f writer