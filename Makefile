# Compiler
CXX = g++
CXXFLAGS = -Wall -Iinclude -Ilibs -Ilibs/external # Include directories

# Source files
SRC = src/main.cpp src/lifxFuncts.cpp src/helperFuncts.cpp src/networkComFuncts.cpp
OBJ = $(SRC:.cpp=.o)

# Output executable
TARGET = lifx-cli

# Build rules
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $(TARGET)

# Compile source files into object files
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f src/*.o $(TARGET)
