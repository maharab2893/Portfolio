CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude
LDFLAGS  := -lcrypto -lz

SRC_DIR  := src
OBJ_DIR  := build
SRCS     := $(wildcard $(SRC_DIR)/*.cpp)
OBJS     := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

# OS detection for binary name and clean commands
ifeq ($(OS),Windows_NT)
    TARGET := mikit.exe
    MKDIR  := if not exist $(OBJ_DIR) mkdir $(OBJ_DIR)
    RM     := del /f /q
    RMDIR  := if exist $(OBJ_DIR) rmdir /s /q $(OBJ_DIR)
else
    TARGET := mikit
    MKDIR  := mkdir -p $(OBJ_DIR)
    RM     := rm -f
    RMDIR  := rm -rf $(OBJ_DIR)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(MKDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(TARGET)
	$(RMDIR)

.PHONY: all clean