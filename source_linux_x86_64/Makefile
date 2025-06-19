# Makefile for PC client program
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=199309L
LDFLAGS = -lrt -lpthread

# 目标文件
TARGET = v4l2_usb_pc
SOURCE = v4l2_usb_pc.c

# 默认目标
all: $(TARGET)

# 编译规则
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

# 清理
clean:
	rm -f $(TARGET)
	rm -rf received_frames/
	@echo "Clean complete"

# 安装（可选）
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/
	@echo "Installed $(TARGET) to /usr/local/bin/"

# 测试连接
test: $(TARGET)
	./$(TARGET) --help

# 创建接收目录
setup:
	mkdir -p received_frames
	@echo "Created received_frames directory"

.PHONY: all clean install test setup
