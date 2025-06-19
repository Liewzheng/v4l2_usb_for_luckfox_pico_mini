# Windows Cross-Compilation on Ubuntu

本目录包含在Ubuntu Linux环境下交叉编译Windows版本的工具和脚本。

## 系统要求

### Ubuntu/Debian 系统
```bash
# 安装 MinGW-w64 交叉编译工具链
sudo apt update
sudo apt install gcc-mingw-w64-x86-64

# 可选：安装 Wine 用于测试
sudo apt install wine
```

### 其他 Linux 发行版
```bash
# CentOS/RHEL/Fedora
sudo yum install mingw64-gcc  # 或 dnf install mingw64-gcc

# Arch Linux
sudo pacman -S mingw-w64-gcc
```

## 编译方法

### 方法1: 使用自动化脚本（推荐）
```bash
chmod +x build_windows.sh
./build_windows.sh
```

### 方法2: 使用 Makefile
```bash
make all
# 或
make help  # 查看所有可用选项
```

### 方法3: 手动编译
```bash
x86_64-w64-mingw32-gcc -Wall -Wextra -O2 -std=c99 -D_WIN32_WINNT=0x0601 \
    -o v4l2_usb_pc.exe ../v4l2_usb_pc_win.c -lws2_32 -static-libgcc

x86_64-w64-mingw32-strip v4l2_usb_pc.exe
```

## 文件说明

- `Makefile` - 交叉编译的 Makefile
- `build_windows.sh` - 自动化编译脚本
- `v4l2_usb_pc.exe` - 编译生成的 Windows 可执行文件（编译后）

## 编译选项说明

### 编译器标志
- `-D_WIN32_WINNT=0x0601` - 支持 Windows 7+ API
- `-static-libgcc` - 静态链接 GCC 运行时，减少依赖
- `-lws2_32` - 链接 Windows Socket 库

### 优化选项
- `-O2` - 编译优化
- `-Wall -Wextra` - 启用警告
- `strip` - 移除调试符号，减小文件大小

## 测试

### 在 Linux 上测试（使用 Wine）
```bash
# 安装 Wine
sudo apt install wine

# 测试程序
wine v4l2_usb_pc.exe --help
```

### 在 Windows 上测试
将 `v4l2_usb_pc.exe` 复制到 Windows 系统：
```cmd
v4l2_usb_pc.exe --help
v4l2_usb_pc.exe -s 172.32.0.100 -p 8888
```

## 故障排除

### 常见问题

1. **编译器未找到**
```
x86_64-w64-mingw32-gcc: command not found
```
**解决方案：** 安装 MinGW-w64
```bash
sudo apt install gcc-mingw-w64-x86-64
```

2. **源文件未找到**
```
error: ../v4l2_usb_pc_win.c: No such file or directory
```
**解决方案：** 确保 `v4l2_usb_pc_win.c` 在父目录或当前目录

3. **链接错误**
```
undefined reference to 'WSAStartup'
```
**解决方案：** 确保链接了 `-lws2_32`

### 调试信息

查看编译详情：
```bash
make V=1  # 显示详细编译命令
```

查看生成的可执行文件信息：
```bash
file v4l2_usb_pc.exe
x86_64-w64-mingw32-objdump -p v4l2_usb_pc.exe | grep DLL
```

## 性能优化

### 进一步优化编译
```bash
# 更高级别优化
x86_64-w64-mingw32-gcc -O3 -march=x86-64 -mtune=generic \
    -flto -fuse-linker-plugin \
    -o v4l2_usb_pc.exe ../v4l2_usb_pc_win.c -lws2_32 -static-libgcc
```

### 减小文件大小
```bash
# 更激进的压缩
x86_64-w64-mingw32-strip --strip-all v4l2_usb_pc.exe

# 可选：使用 UPX 压缩器
upx --best v4l2_usb_pc.exe
```

## 使用场景

### 自动化构建
```bash
#!/bin/bash
# CI/CD 脚本示例
cd /path/to/project/source_win_x86_64
./build_windows.sh
scp v4l2_usb_pc.exe user@windows-host:/path/to/deploy/
```

### 批量编译
```bash
# 编译多个平台版本
make -C ../source_linux_x86_64/  # Linux 版本
make -C ../source_win_x86_64/    # Windows 版本
```

---

**更新时间：** 2025年6月19日  
**支持平台：** Ubuntu 18.04+, Debian 10+, 其他 Linux 发行版
