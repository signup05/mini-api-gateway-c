CC := gcc
CFLAGS := -Wall -Wextra -Wpedantic -std=c11 -O2 -Iinclude
LDFLAGS := -pthread
TARGET := mini-api-gateway-c
SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)

.PHONY: all clean run smoke-test

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

smoke-test:
	./scripts/smoke_test.sh

clean:
	rm -f $(TARGET) $(OBJ)
