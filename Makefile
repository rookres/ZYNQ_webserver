
#编译参数
VERSION 	=
CXX			=g++
# CXX			=arm-linux-gnueabi-g++
DEBUG 		=-DUSE_DEBUG
# CFLAGS		=-Wall
CFLAGS		=-W
INCLUDES  	=-I./include
LIB_NAMES 	=-lpthread
# LIB_PATH 	=-L./lib
LIB_PATH 	=

# 源文件目录
SRC_DIR := source
# 头文件目录
INC_DIR := include
# 链接的二进制目标文件目录
OBJ_DIR := object
# 最终可执行文件目录
BUILD_DIR :=build
# 最终生成的文件
TARGET	=webserver
# 获取所有的.c和.cpp文件
# SOURCES	 	:=$(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/*.cpp)
# 根据 .c和.cpp文件生成对应的 .o 文件
# OBJS    	:= $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCE)))
C_SOURCES := $(wildcard $(SRC_DIR)/*.c)
CPP_SOURCES := $(wildcard $(SRC_DIR)/*.cpp)

C_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(C_SOURCES))
CPP_OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(CPP_SOURCES))

OBJS := $(C_OBJECTS) $(CPP_OBJECTS)


#links链接
$(TARGET):$(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(OBJS) $(LIB_PATH) $(LIB_NAMES) -o $(BUILD_DIR)/$(TARGET)$(VERSION)
#	@rm -rf $(OBJ)
#compile编译
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(INCLUDES) $(DEBUG) -c $(CFLAGS) $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(INCLUDES) $(DEBUG) -c $(CFLAGS) $< -o $@

.PHONY:clean
clean:
	@echo "Remove linked and compiled files and dir......"
	rm -rf $(OBJ_DIR) $(BUILD_DIR)





# # 获取所有的 .cpp 文件
# CPP_FILES := $(wildcard $(SRC_DIR)/*.cpp)
# # 获取所有的 .hpp 文件
# HPP_FILES := $(wildcard $(INC_DIR)/*.hpp)

# # 根据 .cpp 文件生成对应的 .o 文件
# OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(CPP_FILES))

# # 如果存在对应的 .cpp 文件，则从 obj 变量中排除对应的 .hpp 文件
# ifneq ($(CPP_FILES),)
#     OBJ := $(filter-out $(patsubst $(INC_DIR)/%.hpp,$(OBJ_DIR)/%.o,$(HPP_FILES)),$(OBJ_FILES))
# else
#     OBJ := $(OBJ_FILES)
# endif

# # 编译目标
# TARGET := myprogram

# # 编译器和编译选项
# CC := g++
# CFLAGS := -Wall -Wextra

# # 默认目标
# all: $(TARGET)

# # 生成目标文件
# $(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
#     @mkdir -p $(@D)
#     $(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

# # 生成可执行文件
# $(TARGET): $(OBJ)
#     $(CC) $(CFLAGS) $(OBJ) -o $@

# # 清理目标文件和可执行文件
# clean:
#     @rm -rf $(OBJ_DIR) $(TARGET)

# .PHONY: all clean
