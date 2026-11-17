CC = gcc
CFLAGS = -Wall -Wextra -Wshadow -Werror -fsanitize=address,undefined -g

TARGET = sop-cake
SRC = sop-cake.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)