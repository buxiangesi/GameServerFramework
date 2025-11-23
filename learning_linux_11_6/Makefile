# ============================================
# GameServerFramework Makefile
# ============================================

# ========== 编译器配置 ==========
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -I./include -I./include/network -I./include/common -I./include/server
LDFLAGS = -lpthread

# ========== 目录配置 ==========
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin
OBJ_DIR = $(BUILD_DIR)/obj
LIB_DIR = $(BUILD_DIR)/lib

# ========== 源文件 ==========
NETWORK_SRC = src/network/Socket.cpp src/network/Epoll.cpp
COMMON_SRC =
SERVER_SRC =
MAIN_SRC = src/main.cpp

# ========== 目标文件 ==========
NETWORK_OBJ = $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(NETWORK_SRC))
COMMON_OBJ = $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(COMMON_SRC))
SERVER_OBJ = $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(SERVER_SRC))
MAIN_OBJ = $(OBJ_DIR)/main.o

ALL_OBJ = $(NETWORK_OBJ) $(COMMON_OBJ) $(SERVER_OBJ) $(MAIN_OBJ)

# ========== 目标程序 ==========
TARGET = $(BIN_DIR)/game_server

# ========== 编译目标 ==========
.PHONY: all clean dirs debug release

all: dirs $(TARGET)

# 创建目录
dirs:
	@mkdir -p $(BIN_DIR) $(OBJ_DIR)/network $(OBJ_DIR)/common $(OBJ_DIR)/server

# 链接主程序
$(TARGET): $(ALL_OBJ)
	@echo "Linking: $@"
	@$(CXX) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: $@"

# 编译源文件
$(OBJ_DIR)/%.o: src/%.cpp
	@echo "Compiling: $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Debug模式
debug: CXXFLAGS += -g -DDEBUG
debug: all

# Release模式
release: CXXFLAGS += -O2 -DNDEBUG
release: all

# 清理
clean:
	@echo "Cleaning build files..."
	@rm -rf $(BUILD_DIR)
	@echo "Clean complete"

# 运行
run: all
	@echo "Running $(TARGET)..."
	@$(TARGET)

# 帮助
help:
	@echo "GameServerFramework Makefile"
	@echo ""
	@echo "Usage:"
	@echo "  make              - Build the project (default)"
	@echo "  make debug        - Build with debug symbols"
	@echo "  make release      - Build optimized version"
	@echo "  make clean        - Remove build files"
	@echo "  make run          - Build and run the server"
	@echo "  make help         - Show this help message"
	@echo ""
