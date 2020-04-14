
include config.mk
all:
# -C是指定目录
#make -C signal

#可执行文件应该放在最后
# make -C app

#shell命令for shell里边的变量用两个$$
	@for dir in $(BUILD_DIR); \
	do \
		make -C $$dir; \
	done

clean:
	rm -rf app/link_obj app/dep nginx
	rm -rf signal/*.gch app/*.gch
