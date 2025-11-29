.DEFAULT_GOAL = app

# --- Paths ---
BUILD_DIR = $(ASS_HOME)/build

NAME      = ass
BINARY    = $(BUILD_DIR)/$(NAME)

INC_PATH  = $(ASS_HOME)/include
OBJ_DIR   = $(BUILD_DIR)/obj

VALGRIND_LOG = $(BUILD_DIR)/valgrind.log

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
INCLUDES  = -I $(INC_PATH)
CFLAGS   := -O2 -MMD -Wall -Werror $(INCLUDES) -g $(CFLAGS)
LIBS     := -lreadline -ldl -lcurl
LDFLAGS  := -O2 $(LDFLAGS) $(LIBS)

# Execute parameters
ARGS     ?= -l $(BUILD_DIR)/ass.log
ASS_EXEC := $(BINARY) $(ARGS)
ifdef mainargs
ASS_EXEC += $(mainargs)
endif

# --- Rules ---
.PHONY: app clean run gdb valgrind
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

# Execute app
run: app
	$(ASS_EXEC)

gdb: app
	gdb -s $(BINARY) --args $(ASS_EXEC)

# Run app with Valgrind for memory checks
valgrind: app
	@echo "+ Valgrind Interactive Mode (Report Log: $(VALGRIND_LOG))"
	@mkdir -p $(BUILD_DIR)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=$(VALGRIND_LOG) $(ASS_EXEC)
	@echo "+ Valgrind finished. Memory report saved in $(VALGRIND_LOG)."

# Cleanup rule: Removes all generated build files and directories
clean:
	-rm -rf $(BUILD_DIR)