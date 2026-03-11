CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2

TARGET   := vm
INCLUDE_DIR := include
BUILD_DIR := build
SRC_DIR := src
HDRS := $(INCLUDE_DIR)/opcodes.h $(INCLUDE_DIR)/value.h $(INCLUDE_DIR)/vm.h $(INCLUDE_DIR)/assembler.h
OBJS := $(BUILD_DIR)/vm.o $(BUILD_DIR)/assembler.o $(BUILD_DIR)/main.o

.PHONY: all clean run

all: $(BUILD_DIR) $(BUILD_DIR)/$(TARGET) 

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

clean:
	rm -rf $(BUILD_DIR)
