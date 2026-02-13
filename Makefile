CXX      := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
LDFLAGS  :=

TARGET   := vm
SRCS     := main.cpp vm.cpp assembler.cpp
HDRS     := opcodes.h value.h vm.h assembler.h
OBJS     := $(SRCS:.cpp=.o)

.PHONY: all clean run debug

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

debug: CXXFLAGS += -g -O0 -DDEBUG
debug: clean $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
