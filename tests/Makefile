# Makefile for building and running tests

# Compiler and flags
CC = cc
CFLAGS = -std=c17 -Wall -Wextra -pedantic -I../src -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib -lcjson -lncurses

# Source files
TEST_SRCS = test_date_parser.c test_utils.c
SRC_FILES = ../src/date_parser.c

# Object files
TEST_OBJS = $(TEST_SRCS:.c=.o)
SRC_OBJS = $(notdir $(SRC_FILES:.c=.o))

# Test executable
TEST_TARGET = test_date_parser

# Default target
.PHONY: all test clean

all: $(TEST_TARGET)

# Main test target
test: $(TEST_TARGET)
	@echo "Running tests..."
	@./$(TEST_TARGET)

# Link test executable
$(TEST_TARGET): $(TEST_OBJS) $(SRC_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile test files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile source files from src directory
%.o: ../src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f $(TEST_TARGET) $(TEST_OBJS) $(SRC_OBJS) $(TEST_UTILS) $(TEST_UTILS_OBJ)

# Run tests with verbose output
check: test

# Alias for test
t: test
