# Define directories
PROGNAME := "Output"
CUR_DIR := .
INC_DIR := $(CUR_DIR)/inc
SRC_DIR := $(CUR_DIR)/src
BIN_DIR := $(CUR_DIR)/bin
OBJ_DIR := $(CUR_DIR)/obj
LIB_DIR := $(CUR_DIR)/lib
OUTDIR := $(CUR_DIR)/out
PROG := $(OUTDIR)/$(PROGNAME)
LIB_SHARED_DIR := $(LIB_DIR)/shared
LIB_STATIC_DIR := $(LIB_DIR)/static

# Compiler flags
FLAGS := -I $(INC_DIR)

# List of c files
CSRCS := \
$(SRC_DIR)/main.c \
$(SRC_DIR)/hellolinux.c \
$(SRC_DIR)/helloworld.c

OBJS_i := $(CSRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.i)
OBJS_s := $(CSRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.s)
OBJS_o := $(CSRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Preprocessing
preprocessing: $(OBJS_i)

$(OBJ_DIR)/%.i: $(SRC_DIR)/%.c
	mkdir -p $(OBJ_DIR)
	gcc $(FLAGS) -E $< -o $@

# Compiling (creating assembly code)
compilation: $(OBJS_s)

$(OBJ_DIR)/%.s: $(SRC_DIR)/%.c
	mkdir -p $(OBJ_DIR)
	gcc $(FLAGS) -S $< -o $@

# Assembling (creating object files)
assembly: $(OBJS_o)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(OBJ_DIR)
	gcc $(FLAGS) -c $< -o $@

# Create shared library
create_shared_lib: $(OBJS_o)
	mkdir -p $(LIB_SHARED_DIR)
	gcc -shared $(OBJ_DIR)/hellolinux.o $(OBJ_DIR)/helloworld.o -o $(LIB_SHARED_DIR)/libhello.so

# Create static library
create_static_lib: $(OBJS_o)
	mkdir -p $(LIB_STATIC_DIR)
	ar rcs $(LIB_STATIC_DIR)/libhello.a $(OBJ_DIR)/hellolinux.o $(OBJ_DIR)/helloworld.o

# Build all (link with shared library)
all_shared: $(OBJ_DIR)/main.o create_shared_lib
	mkdir -p $(BIN_DIR)
	gcc $(OBJ_DIR)/main.o -L$(LIB_SHARED_DIR) -lhello -o $(BIN_DIR)/use-shared-library
	mkdir -p $(OUTDIR)
	cp $(BIN_DIR)/use-shared-library $(OUTDIR)/output

# Build all (link with static library)
all_static: $(OBJ_DIR)/main.o create_static_lib
	mkdir -p $(BIN_DIR)
	gcc $(OBJ_DIR)/main.o -L$(LIB_STATIC_DIR) -lhello -o $(BIN_DIR)/use-static-library
	mkdir -p $(OUTDIR)
	cp $(BIN_DIR)/use-static-library $(OUTDIR)/output

# Default build (all)
all: preprocessing compilation assembly create_shared_lib all_shared

run_shared:
	LD_LIBRARY_PATH=$(LIB_SHARED_DIR) $(BIN_DIR)/use-shared-library

run_static:
	LD_LIBRARY_PATH=$(LIB_STATIC_DIR) $(BIN_DIR)/use-static-library

# Clean output files
clean:
	rm -rf $(OBJ_DIR)/*.i $(OBJ_DIR)/*.s $(OBJ_DIR)/*.o
	rm -rf $(LIB_SHARED_DIR)/*.so
	rm -rf $(LIB_STATIC_DIR)/*.a
	rm -rf $(BIN_DIR)/use-shared-library $(BIN_DIR)/use-static-library
	rm -rf $(BIN_DIR)/use-shared-library $(BIN_DIR)/use-shared-library

# Phony targets
.PHONY: all all_shared all_static clean preprocessing compilation assembly create_shared_lib create_static_lib run
