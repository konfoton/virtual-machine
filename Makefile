CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2

TARGET   := vm
INCLUDE_DIR := include
BUILD_DIR := build
SRC_DIR := src
TEST_DIR := tests

HDRS := $(INCLUDE_DIR)/opcodes.h $(INCLUDE_DIR)/value.h $(INCLUDE_DIR)/vm.h $(INCLUDE_DIR)/assembler.h
OBJS := $(BUILD_DIR)/vm.o $(BUILD_DIR)/assembler.o $(BUILD_DIR)/main.o

GTEST_DIR := libs/googletest
GTEST_INC := $(GTEST_DIR)/googletest/include
GTEST_SRC := $(GTEST_DIR)/googletest/src
GTEST_FLAGS := -I$(GTEST_INC) -I$(GTEST_DIR)/googletest
GTEST_OBJ := $(BUILD_DIR)/gtest-all.o

TEST_OBJS := $(BUILD_DIR)/vm.o $(BUILD_DIR)/assembler.o
TEST_TARGET := $(BUILD_DIR)/run_tests

.PHONY: all clean run test

all: $(BUILD_DIR) $(BUILD_DIR)/$(TARGET) 


# Building the main application
$(BUILD_DIR):
	mkdir -p $@
$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD_DIR)/vm.o: $(SRC_DIR)/vm.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

$(BUILD_DIR)/assembler.o: $(SRC_DIR)/assembler.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

$(BUILD_DIR)/main.o: $(SRC_DIR)/main.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

run: $(BUILD_DIR)/$(TARGET)
	./$(BUILD_DIR)/$(TARGET)


# Google Test
# Compile Google Test library
$(GTEST_OBJ): $(GTEST_SRC)/gtest-all.cc | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(GTEST_FLAGS) -c $< -o $@

# Build test executable
$(TEST_TARGET): $(TEST_OBJS) $(BUILD_DIR)/test_main.o $(GTEST_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ -pthread

# Compile test files
$(BUILD_DIR)/test_main.o: $(TEST_DIR)/test_main.cpp $(HDRS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) $(GTEST_FLAGS) -c $< -o $@

# Run tests
test: $(BUILD_DIR) $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -rf $(BUILD_DIR)
