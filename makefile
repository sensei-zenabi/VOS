# Compiler and flags
CC = gcc
CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200112L `sdl2-config --cflags`
LDFLAGS = `sdl2-config --libs`

# Source files
SRC = main.c $(wildcard components/*.c)
OBJ = $(SRC:.c=.o)

# Target executable
TARGET = vos

# Default rule
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build
clean:
	rm -f $(TARGET) $(OBJ)
