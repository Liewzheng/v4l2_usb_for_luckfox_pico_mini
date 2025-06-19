# Windows 编译说明

## 在 Windows 上编译 V4L2 USB PC 客户端

### 前置要求

在 Windows 上编译需要安装 C 编译器。推荐以下选项之一：

#### 1. MinGW-w64 (推荐)
- 下载地址: https://www.mingw-w64.org/downloads/
- 或使用 MSYS2: https://www.msys2.org/
- 安装后确保 `gcc` 在 PATH 中

#### 2. TDM-GCC
- 下载地址: https://jmeubank.github.io/tdm-gcc/
- 安装简单，自动配置 PATH

#### 3. Microsoft Visual Studio Community (可选)
- 使用 Developer Command Prompt
- 需要修改编译命令

### 编译方法

#### 方法1: 使用批处理脚本 (推荐)
```cmd
build_windows.bat
```

#### 方法2: 使用 Make
```cmd
mingw32-make -f Makefile.win
```

#### 方法3: 手动编译
```cmd
gcc -Wall -Wextra -O2 -std=c99 -o v4l2_usb_pc.exe v4l2_usb_pc.c -lws2_32
```

### 运行程序

编译成功后会生成 `v4l2_usb_pc.exe`：

```cmd
REM 查看帮助
v4l2_usb_pc.exe --help

REM 连接到默认地址 (172.32.0.93:8888)
v4l2_usb_pc.exe

REM 指定服务器地址
v4l2_usb_pc.exe -s 172.32.0.93 -p 8888

REM 指定输出目录
v4l2_usb_pc.exe -s 172.32.0.93 -o ./frames
```

### Windows 特定注意事项

1. **路径分隔符**: 
   - 使用正斜杠: `./frames` 
   - 或双反斜杠: `.\\\\frames`
   - 避免单反斜杠: `.\\frames` (会被转义)

2. **防火墙设置**:
   - Windows 防火墙可能阻止网络连接
   - 首次运行时选择"允许访问"

3. **网络配置**:
   - 确保 Windows 和 Luckfox Pico 在同一网络
   - 检查 IP 地址配置 (默认: 172.32.0.93)

### 故障排除

#### 编译错误
```
error: sys/socket.h: No such file or directory
```
**解决**: 使用 `v4l2_usb_pc.c` 而不是 `v4l2_usb_pc.c`

#### 运行时错误
```
WSAStartup failed: 10091
```
**解决**: 确保系统支持 Windows Socket 2.2

#### 连接失败
```
connect failed: 10061
```
**解决**: 
- 检查服务器是否运行
- 确认 IP 地址和端口正确
- 检查防火墙设置

### 性能优化

1. **编译优化**:
   ```cmd
   gcc -O3 -march=native -o v4l2_usb_pc.exe v4l2_usb_pc.c -lws2_32
   ```

2. **运行时优化**:
   - 使用 SSD 作为输出目录
   - 关闭不必要的后台程序
   - 使用有线网络连接

### 示例输出

成功运行时的输出：
```
V4L2 USB RAW Image Receiver (Cross-Platform PC Client)
=====================================================
Server: 172.32.0.93:8888
Output: ./received_frames
Windows Socket initialized
Created output directory: ./received_frames
Connecting to 172.32.0.93:8888...
Connected successfully!
Starting receive loop (Ctrl+C to stop)...
Frames will be saved to: ./received_frames
Allocated 3317760 bytes frame buffer
Frame 0: 2048x1296, pixfmt=0x30314742 (BG10), size=3317760 bytes, timestamp=0.000s
  -> Saved to file
Frame 1: 2048x1296, pixfmt=0x30314742 (BG10), size=3317760 bytes, timestamp=0.031s
...
```
