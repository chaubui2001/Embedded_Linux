#
# File: Makefile
# Author: Chau Bui
# Description: This Makefile is used to build the chat application project.
#              It defines the compiler, flags, source files, and rules to compile
#              and link the project into an executable.


# Compiler and flags
# CC: Specifies the compiler to use (GNU C Compiler)
# CFLAGS: Compiler flags for warnings, threading support, and header inclusion
CC = gcc
CFLAGS = -Wall -Wextra -pthread -Iinc

# Source files
# SRCS: List of all source files to be compiled
SRCS = src/chat.c src/client.c src/server.c src/utils.c src/connection_manager.c

# Object files
# OBJS: Converts source file paths to object file paths in build/obj directory
OBJS = $(patsubst src/%.c, build/obj/%.o, $(SRCS))

# Target executable
# TARGET: Path to the final executable file
TARGET = build/out/chat

# Default rule: build the entire program
# Ensures the target executable is built when 'make' is run
all: $(TARGET)

# Rule to link object files into the executable
# Creates the output directory and links all object files into the executable
$(TARGET): $(OBJS)
	@mkdir -p build/out
	$(CC) $(CFLAGS) -o $@ $^

# Rule to compile .c files into .o files
# Compiles each source file into an object file, creating the obj directory if needed
build/obj/%.o: src/%.c
	@mkdir -p build/obj
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule: remove the build directory
# Removes all compiled files for a clean slate
clean:
	rm -rf build

# Phony targets
# Declares 'all' and 'clean' as non-file targets to avoid conflicts
.PHONY: all clean