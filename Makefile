# Compiler and flags
CXX := g++
CXXFLAGS := -Wall -Wextra -std=c++17 -O2

# Target and source files
TARGET := Lamport_ds
SRC := Lamport_ds.cpp

# Build rules
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean