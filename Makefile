
# 头文件目录
INC_DIR 	:= include
# 源文件目录
SRC_DIR 	:= source
# 链接的二进制目标文件目录
OBJ_DIR 	:= object
# 最终可执行文件目录
BUILD_DIR 	:= build
#********************************#
#编译参数
VERSION 	=
CC		=g++
# CC			=arm-linux-gnueabi-g++
DEBUG 		=-DUSE_DEBUG -g
# DEBUG 		=
CFLAGS		=-Wall -Wextra -O3
# CFLAGS		=-W
INCLUDES  	=-I./$(INC_DIR)
LIB_NAMES 	=-lpthread
# LIB_PATH 	=-L./lib
LIB_PATH 	=

# 最终生成的文件
TARGET		= webserver
#因为暂时没有不需要编译的文件,先置空
# 排除不希望编译的特定文件,如果也不想编译某个目录下的某个文件，例如$(SRC_DIR)/file.c,用空格分隔，例如no.c ./dir/file.c
EXCLUDE_FILES := 	
# 排除不希望编译整个目录及其内容,最好加上./表示当前目录下的目录,不要加/，下面筛选的时候已经加上了,例如./test ./test1
EXCLUDE_DIRS  := 	

SOURCES := $(filter-out $(EXCLUDE_FILES), $(SOURCES))	# 从所有.c文件中移除不需要的单个文件
SOURCES := $(filter-out $(foreach dir, $(EXCLUDE_DIRS), $(wildcard $(dir)/*)), $(SOURCES))	# 移除指定目录及其包含的所有文件

# #获取所有的.c和.cpp文件
# SOURCES	 	:=$(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/*.cpp)
# #根据 .c和.cpp文件生成对应的 .o 文件
# OBJS    	:= $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCE)))
C_SOURCES 	:= $(wildcard $(SRC_DIR)/*.c)
CPP_SOURCES := $(wildcard $(SRC_DIR)/*.cpp)

C_OBJECTS 	:= $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(C_SOURCES))
CPP_OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(CPP_SOURCES))

OBJS 		:= $(C_OBJECTS) $(CPP_OBJECTS)


#links链接
$(TARGET):$(OBJS)
	@mkdir -p $(BUILD_DIR)
	@$(CC) $(OBJS) $(LIB_PATH) $(LIB_NAMES) -o $(BUILD_DIR)/$(TARGET)$(VERSION)
	@echo "***The Compile Successful***"
	@echo "***The Final Executable File in ./$(BUILD_DIR)/$(TARGET)***"

#compile编译
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	@$(CC) $(INCLUDES) $(DEBUG) -c $(CFLAGS) $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	@$(CC) $(INCLUDES) $(DEBUG) -c $(CFLAGS) $< -o $@


#这样直接make rebuild可以一下子执行make clean和make all
.PHONY : all clean rebuild

rebuild: clean all

all : $(TARGET)

clean:
	@rm -rf $(OBJ_DIR) $(BUILD_DIR)
	@echo "***Remove $(OBJ_DIR) and $(BUILD_DIR) folders***"
	@echo "***You Can Use \"make\" Order Compile Again***"