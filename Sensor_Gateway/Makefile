# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude -MMD -MP  # -MMD -MP for auto-dependency generation
LDFLAGS = 
LDLIBS = -lpthread -lsqlite3          # Libraries needed for the gateway
LDLIBS_SIM =                          # Libraries needed for the simulator (none extra for now)

# Directories
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
OUT_DIR = $(BUILD_DIR)/out
SRC_DIR = src
TEST_DIR = test
INCLUDE_DIR = include

# Main gateway executable
TARGET_GATEWAY = $(OUT_DIR)/sensor_gateway

# Test simulator executable
TARGET_SIM = $(OUT_DIR)/sensor_sim

# Client command
TARGET_CLIENT = $(OUT_DIR)/cmd_client

# Find all .c source files in the src directory
SOURCES_GATEWAY = $(wildcard $(SRC_DIR)/*.c)
# Generate corresponding object file paths in the object directory
OBJECTS_GATEWAY = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES_GATEWAY))
# Generate dependency file paths
DEPS_GATEWAY = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.d, $(SOURCES_GATEWAY))

# Simulator source and object file
SOURCES_SIM = $(TEST_DIR)/sensor_sim.c
OBJECTS_SIM = $(patsubst $(TEST_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES_SIM))
DEPS_SIM = $(patsubst $(TEST_DIR)/%.c, $(OBJ_DIR)/%.d, $(SOURCES_SIM))

# Client command source and object file
SOURCES_CLIENT = $(SRC_DIR)/cmd_client.c
OBJECTS_CLIENT = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES_CLIENT))
DEPS_CLIENT = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.d, $(SOURCES_CLIENT))

# Phony targets (targets that don't represent files)
.PHONY: all test client clean

# Default target: Build the main sensor gateway
all: $(TARGET_GATEWAY)

# Target to build only the test simulator
test: $(TARGET_SIM)

# Target to build only the client command
client: $(TARGET_CLIENT)

# Rule to link the main sensor gateway executable
$(TARGET_GATEWAY): $(OBJECTS_GATEWAY)
	@mkdir -p $(OUT_DIR) # Create output directory if it doesn't exist
	@echo "Linking gateway executable: $@"
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# Rule to link the sensor simulator executable
$(TARGET_SIM): $(OBJECTS_SIM)
	@mkdir -p $(OUT_DIR) # Create output directory if it doesn't exist
	@echo "Linking simulator executable: $@"
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS_SIM)

# Rule to link the client command executable
$(TARGET_CLIENT): $(OBJECTS_CLIENT)
	@mkdir -p $(OUT_DIR) # Create output directory if it doesn't exist
	@echo "Linking client command executable: $@"
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# Pattern rule to compile gateway source files (.c -> .o)
# $<: The first prerequisite (the .c file)
# $@: The target file (the .o file)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR) # Create object directory if it doesn't exist
	@echo "Compiling gateway source: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to compile the simulator source file (.c -> .o)
$(OBJ_DIR)/sensor_sim.o: $(TEST_DIR)/sensor_sim.c
	@mkdir -p $(OBJ_DIR) # Create object directory if it doesn't exist
	@echo "Compiling simulator source: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to compile the client command source file (.c -> .o)
$(OBJ_DIR)/cmd_client.o: $(TEST_DIR)/cmd_client.c
	@mkdir -p $(OBJ_DIR) # Create object directory if it doesn't exist
	@echo "Compiling client command source: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to clean up build artifacts
clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)
	@echo "Clean done."	

# Include automatically generated dependency files
# The minus sign ignores errors if dependency files don't exist yet
-include $(DEPS_GATEWAY)
-include $(DEPS_SIM)