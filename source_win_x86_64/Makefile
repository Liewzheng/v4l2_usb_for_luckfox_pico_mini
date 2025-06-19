# Makefile for Windows cross-compilation on Ubuntu using MinGW-w64
# Install: sudo apt install gcc-mingw-w64-x86-64

CC = x86_64-w64-mingw32-gcc
STRIP = x86_64-w64-mingw32-strip
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_WIN32_WINNT=0x0601
LDFLAGS = -lws2_32 -static-libgcc -static-libstdc++

# 目标文件
TARGET = v4l2_usb_pc.exe
SOURCE = v4l2_usb_pc.c

# 检查源文件是否存在
ifeq (,$(wildcard $(SOURCE)))
    SOURCE = ../v4l2_usb_pc.c
endif

# 默认目标
all: check-compiler $(TARGET)

# 检查编译器是否安装
check-compiler:
	@which $(CC) > /dev/null || (echo "Error: MinGW-w64 not found. Install with: sudo apt install gcc-mingw-w64-x86-64" && exit 1)
	@echo "Using compiler: $(CC)"

# 编译规则
$(TARGET): $(SOURCE)
	@echo "Cross-compiling for Windows x86_64..."
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)
	$(STRIP) $(TARGET)
	@echo "Build complete: $(TARGET)"
	@echo "File size: $$(du -h $(TARGET) | cut -f1)"

# 清理 (Linux命令)
clean:
	rm -f $(TARGET)
	rm -rf received_frames/
	@echo "Clean complete"

# 测试 (需要wine)
test: $(TARGET)
	@which wine > /dev/null && wine $(TARGET) --help || echo "Install wine to test: sudo apt install wine"

# 创建接收目录
setup:
	mkdir -p received_frames
	@echo "Created received_frames directory"

# 安装 MinGW-w64 工具链
install-mingw:
	sudo apt update
	sudo apt install gcc-mingw-w64-x86-64 wine
	@echo "MinGW-w64 and Wine installed"

# 显示帮助
help:
	@echo "Available targets:"
	@echo "  all          - Build Windows executable"
	@echo "  clean        - Remove build files"
	@echo "  test         - Test with Wine (requires wine)"
	@echo "  setup        - Create output directory"
	@echo "  install-mingw- Install MinGW-w64 tools"
	@echo "  help         - Show this help"

.PHONY: all clean test setup install-mingw help check-compiler
