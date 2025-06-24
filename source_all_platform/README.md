# V4L2 USB PC Client v2.0

跨平台V4L2图像流接收客户端，支持Windows、Linux和macOS。

## 特性

- 🌐 **跨平台支持**: Windows x86_64、Linux x86_64、macOS ARM64
- 📚 **模块化架构**: 分离头文件、核心库和主程序
- 🔧 **CMake构建系统**: 支持静态库、动态库和可执行文件
- ⚡ **高性能处理**: 多线程SBGGR10解包，SIMD优化
- 🎛️ **灵活配置**: 可选择启用/禁用格式转换

## 项目结构

```
├── v4l2_usb_pc.h           # 头文件：接口定义和数据结构
├── v4l2_usb_pc_core.c      # 核心库：网络通信、图像处理
├── v4l2_usb_pc_main.c      # 主程序：命令行解析和程序入口
├── CMakeLists.txt          # CMake构建配置
├── build_all_platforms.sh  # 跨平台构建脚本
└── README.md          # 本文档
```

## 快速开始

### 1. 原生平台构建

```bash
# 构建当前平台版本
./build_all_platforms.sh native

# 或使用CMake直接构建
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### 2. 跨平台构建

```bash
# 构建所有支持的平台
./build_all_platforms.sh all

# 仅构建Windows版本
./build_all_platforms.sh windows

# 仅构建macOS版本（实验性）
./build_all_platforms.sh macos
```

### 3. 安装交叉编译工具链

```bash
# Windows交叉编译（Ubuntu/Debian）
sudo apt install gcc-mingw-w64-x86-64

# macOS交叉编译（需要osxcross）
# 参考：https://github.com/tpoechtrager/osxcross
```

## 使用方法

### 基本用法

```bash
# 连接到默认服务器，仅保存RAW文件
./v4l2_usb_pc -s 172.32.0.93 -p 8888

# 启用SBGGR10转换，保存RAW+解包文件
./v4l2_usb_pc -s 172.32.0.93 -c

# 每5帧保存一次，启用转换
./v4l2_usb_pc -s 172.32.0.93 -c -i 5
```

### 命令行参数

- `-s, --server IP`: 服务器IP地址 (默认: 172.32.0.93)
- `-p, --port PORT`: 服务器端口 (默认: 8888)
- `-o, --output DIR`: 输出目录 (默认: ./received_frames)
- `-c, --convert`: 启用SBGGR10到16位转换 (默认: 禁用)
- `-i, --interval N`: 保存间隔，每N帧保存一次 (默认: 1)
- `-h, --help`: 显示帮助信息

## 构建输出

构建完成后，文件将按平台组织在`dist/`目录下：

```
build_*/dist/
├── linux_x86_64/
│   ├── bin/v4l2_usb_pc          # 可执行文件
│   └── lib/
│       ├── libv4l2_usb_pc.so    # 动态库
│       └── libv4l2_usb_pc_static.a  # 静态库
├── windows_x86_64/
│   ├── bin/v4l2_usb_pc.exe      # Windows可执行文件
│   └── lib/
│       ├── v4l2_usb_pc.dll      # 动态库
│       └── v4l2_usb_pc_static.a # 静态库
└── macos_arm64/
    ├── bin/v4l2_usb_pc          # macOS可执行文件
    └── lib/
        ├── libv4l2_usb_pc.dylib # 动态库
        └── libv4l2_usb_pc_static.a  # 静态库
```

## 性能优化

- **内存池**: 预分配8MB缓冲区，减少malloc/free开销
- **多线程**: 根据CPU核心数自动调整线程数量
- **SIMD加速**: 自动检测并启用AVX2/SSE2指令集
- **减少日志**: 仅在前3次和每50次解包时输出详细信息

## API使用

如果您想将核心功能集成到其他项目中：

```c
#include "v4l2_usb_pc.h"

// 初始化
init_network();
init_memory_pool();

// 连接服务器
socket_t sock = connect_to_server("172.32.0.93", 8888);

// 接收和处理数据
struct client_config config = {
    .server_ip = "172.32.0.93",
    .port = 8888,
    .output_dir = "./frames",
    .enable_conversion = 1,
    .save_interval = 1
};
receive_loop(sock, &config);

// 清理
close_socket(sock);
cleanup_memory_pool();
cleanup_network();
```

## 系统要求

- **Linux**: GCC 7+, CMake 3.16+
- **Windows**: MinGW-w64 或 MSVC 2019+
- **macOS**: Clang, Xcode Command Line Tools
- **内存**: 最少16MB可用内存
- **网络**: TCP/IP连接能力

## 故障排除

### 编译错误

1. **CMake版本过低**: 升级到3.16+
2. **编译器不支持**: 检查GCC/Clang版本
3. **SIMD指令错误**: 禁用`-march=native`

### 运行时错误

1. **连接失败**: 检查服务器IP和端口
2. **内存不足**: 减少缓冲区大小
3. **文件权限**: 确保输出目录可写

### 性能问题

1. **转换慢**: 确保启用了SIMD优化
2. **内存泄漏**: 检查内存池是否正确清理
3. **网络延迟**: 调整接收超时时间

## 更新日志

### v2.0.0 (2025-06-24)
- 🔄 重构为模块化架构
- 🏗️ 迁移到CMake构建系统  
- 🌐 添加多平台交叉编译支持
- ⚡ 性能优化：内存池、减少日志
- 🎛️ 可选SBGGR10转换功能

### v1.0.0
- 基础功能实现
- Makefile构建系统
- SBGGR10解包支持
