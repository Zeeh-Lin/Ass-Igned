.DEFAULT_GOAL = app

# --- Paths ---
WORK_DIR  = $(shell pwd)
BUILD_DIR = $(WORK_DIR)/build

NAME      = Ass_Igned
BINARY    = $(BUILD_DIR)/$(NAME)

INC_PATH  = $(WORK_DIR)/include
OBJ_DIR   = $(BUILD_DIR)/obj

# --- Sources & Objects ---
# Find all .c files recursively under the src directory
SRCS = $(shell find src -name "*.c")

# Map source files to object files (e.g., src/utils/log.c -> build/obj/src/utils/log.o)
OBJS = $(SRCS:%.c=$(OBJ_DIR)/%.o)

# --- Configuration ---
CC       = gcc
LD       = $(CC)

# CFLAGS: Optimization (-O2), Dependency generation (-MMD), Strict warnings (-Wall), 
# Treat warnings as errors (-Werror), Debug info (-g), Include paths
INCLUDES = -I $(INC_PATH)
CFLAGS  := -O2 -MMD -Wall -Werror $(INCLUDES) -g $(CFLAGS)
LDFLAGS := -O2 $(LDFLAGS)

# --- Rules ---
.PHONY: app clean
app: $(BINARY)

# Linking rule: Generates the final executable
$(BINARY): $(OBJS)
	@echo "+ LD $@"
	@mkdir -p $(dir $@)
	$(LD) -o $@ $(OBJS) $(LDFLAGS)

# Compilation rule: Compiles C source files into object files
$(OBJ_DIR)/%.o: %.c
	@echo "+ CC $<"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# Dependencies (automatic handling of header file changes)
-include $(OBJS:.o=.d)

# Cleanup rule: Removes all generated build files and directories
clean:
	@echo "+ Clean $(BUILD_DIR)"
	-rm -rf $(BUILD_DIR)