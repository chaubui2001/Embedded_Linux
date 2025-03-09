# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -g

# Target executable name
TARGET = Test

# Source files
SOURCES = Test.c

# Object files (automatically generated from SOURCES)
OBJECTS = $(SOURCES:.c=.o)

# Default target (build the executable)
all: $(TARGET)

# Rule to build the executable
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET)

# Rule to compile a .c file into a .o file
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule (remove object files and executable)
clean:
	rm -f $(OBJECTS) $(TARGET)

# Phony targets (targets that are not actual files)
.PHONY: all clean