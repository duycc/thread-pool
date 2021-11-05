# c: thread-pool

CC = gcc

TARGET = pool

SRCS = $(wildcard *.c)
INCLUDE_PATH = -I ./
LIBS = -lpthread

$(TARGET):$(SRCS)
	$(CC) $^ $(INCLUDE_PATH) $(LIBS) -o $@

.PHONY: clean
clean:
	-rm -rf $(TARGET)
