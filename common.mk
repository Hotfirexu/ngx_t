#.PHONY:all clean

ifeq ($(DEBUG),true)
CC = g++ -g 
VERSION = debug 
else
CC = g++ 
VERSION = release
endif


# $(wildcard *.c)表示扫描当前目录下的所有的.c文件
SRCS = $(wildcard *.cpp)

#把字符串中的.c全部换成.o
OBJS = $(SRCS:.cpp=.o)

#把字符串中的.c全部换成.d
DEPS = $(SRCS:.cpp=.d)


#可以指定BIN文件的位置， addprefix是增减前缀函数
BIN := $(addprefix $(BUILD_ROOT)/,$(BIN))

LINK_OBJ_DIR = $(BUILD_ROOT)/app/link_obj
DEP_DIR = $(BUILD_ROOT)/app/dep

#创建目录
$(shell mkdir -p $(LINK_OBJ_DIR))
$(shell mkdir -p $(DEP_DIR))

# := 在解析阶段直接赋值常亮字符串【立即展开】，而 = 在运行阶段，实际使用变量时在进行求值【延迟展开】
OBJS := $(addprefix $(LINK_OBJ_DIR)/,$(OBJS))
DEPS := $(addprefix $(DEP_DIR)/,$(DEPS))

#找到目录中所有的.o文件【编译出来的】
LINK_OBJ = $(wildcard $(LINK_OBJ_DIR)/*.o)
#因为构建依赖关系时app目录下的这个.o文件还没构建出来，所以LINK_OBJ是缺少这个.o的， 需要手动加进来
LINK_OBJ += $(OBJS)


all:$(DEPS) $(OBJS) $(BIN)

ifneq ("$(wildcard $(DEPS))","")
include $(DEPS)
endif

$(BIN):$(LINK_OBJ)
	@echo "========== build $(VERSION) mode ========== !!!"
	# $@:目标   $^:所有目标依赖
	$(CC) -o $@ $^

$(LINK_OBJ_DIR)/%.o:%.cpp
	#gcc -c 是生成.o目标文件  -I可以指定头文件的路径
	#如下不排除其他字符串，所以从其中专门把.c过滤出来
	$(CC) -I$(INCLUDE_PATH) -o $@ -c $(filter %.cpp,$^)

#
$(DEP_DIR)/%.d:%.cpp
	#echo 中的-n表示后续追加不换行
	echo -n $(LINK_OBJ_DIR)/ > $@
	g++ -I$(INCLUDE_PATH) -MM $^ >> $@






