# Define variables
CC = gcc
CFLAGS = -Wall -Ilib 
SRC_DIR = src
LIB_DIR = lib
OUTPUT_DIR = output

# Preprocessing
preprocessing: $(OUTPUT_DIR)/main.i

$(OUTPUT_DIR)/main.i: $(SRC_DIR)/main.c $(LIB_DIR)/hello.h
	mkdir -p $(OUTPUT_DIR)
	$(CC) $(CFLAGS) -E $< -o $@

# Compiling (creating assembly code)
compiling: $(OUTPUT_DIR)/main.s

$(OUTPUT_DIR)/main.s: $(SRC_DIR)/main.c $(LIB_DIR)/hello.h
	mkdir -p $(OUTPUT_DIR)
	$(CC) $(CFLAGS) -S $< -o $@

# Assembling (creating object file)
assembling: $(OUTPUT_DIR)/main.o

$(OUTPUT_DIR)/main.o: $(OUTPUT_DIR)/main.s
	$(CC) $(CFLAGS) -c $< -o $@

# Linking
linking: $(OUTPUT_DIR)/main

$(OUTPUT_DIR)/main: $(OUTPUT_DIR)/main.o
	$(CC) $< -o $@

# Rule to clean intermediate and executable files
clean:
	rm -f $(OUTPUT_DIR)/*.i $(OUTPUT_DIR)/*.s $(OUTPUT_DIR)/*.o $(OUTPUT_DIR)/main

# Build the final executable
all: $(OUTPUT_DIR)/main

$(OUTPUT_DIR)/main: $(SRC_DIR)/main.c $(LIB_DIR)/hello.h
	$(CC) $(CFLAGS) -o $@ $< 

.PHONY: all clean preprocessing compiling assembling linking