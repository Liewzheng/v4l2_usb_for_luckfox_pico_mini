# Luckfox Pico V4L2 摄像头高性能采集与网络推流系统 v2.0.0

一个专为 **L## 🚀 快速开始

### 1. 系统架构

```
┌─────────────────┐    网络推流     ┌─────────────────┐
│   Luckfox Pico  │ ────────────► │   PC 客户端      │
│                 │   TCP:8888    │                 │
│ • v4l2_usb      │               │ • v4l2_usb_pc   │
│ • SC3336 摄像头 │               │ • 帧数据保存     │
│ • 172.32.0.93   │               │ • 实时显示       │
└─────────────────┘               └─────────────────┘
```

### 2. 预编译版本 (推荐) ⚡

**下载即用发布包**:
- 📥 [PC 端多平台包](source_all_platform/v4l2_usb_pc_v2.0.0_all_platforms.zip) - Windows/Linux/macOS
- 📥 [嵌入式 ARM 包](source_linux_armv7l/v4l2_usb_embedded_v2.0.0_luckfox_pico_armv7l.tar.gz) - Luckfox Pico

**PC 端使用**:
```bash
# 解压对应平台包
tar -xf v4l2_usb_pc_v2.0.0_linux_x86_64.tar.gz    # Linux
# 或 unzip v4l2_usb_pc_v2.0.0_windows_x86_64.zip  # Windows

# 运行客户端
cd linux_x86_64 && ./run.sh                       # Linux/macOS
# 或 cd windows_x86_64 && run.bat                  # Windows
```

**嵌入式端部署**:
```bash
# 传输到设备
scp v4l2_usb_embedded_v2.0.0_luckfox_pico_armv7l.tar.gz root@172.32.0.93:~/
ssh root@172.32.0.93
tar -xf v4l2_usb_embedded_v2.0.0_luckfox_pico_armv7l.tar.gz
cd luckfox_pico_armv7l

# 一键启动
./run_v4l2_usb.sh
```

### 3. 自定义编译 🔨i B + SC3336 RAW10 摄像头** 设计的高性能视频采集与网络推流系统，支持 2304×1296 分辨率实时采集，并通过网络推流至 PC 端进行处理。

## 🎯 项目特色

- **🎥 高分辨率支持**: 2304×1296 RAW10 格式，专业级图像质量
- **⚡ 高性能传输**: 基于多平面 V4L2 API，优化内存对齐与缓冲区管理
- **🌐 全平台兼容**: Linux x86_64、Windows x86_64、macOS ARM64、ARM Linux
- **🚀 自动化构建**: 一键编译、打包、发布所有平台
- **📦 模块化架构**: 代码分离，易于维护和扩展
- **🔧 即开即用**: 预编译二进制文件，开箱即用
- **🌐 网络推流**: TCP 实时推流，支持多客户端连接
- **💾 零临时文件**: 纯内存传输，避免存储空间限制
- **✨ 优雅退出**: 完善的信号处理，支持 Ctrl+C 正常退出

## 📁 项目结构

```
v4l2_bench/
├── 📦 source_all_platform/           # 🖥️ PC 端跨平台代码 (v2.0.0)
│   ├── v4l2_usb_pc.h                 # 🔗 模块化头文件定义
│   ├── v4l2_usb_pc_core.c            # ⚙️ 核心功能实现
│   ├── v4l2_usb_pc_main.c            # 🚀 主程序入口
│   ├── CMakeLists.txt                # 🔨 CMake 构建配置
│   ├── build_all_platforms.sh        # 🌐 一键构建所有平台
│   ├── create_release.sh             # 📦 自动打包发布脚本
│   ├── toolchains/                   # 🔧 交叉编译工具链
│   │   ├── windows_x86_64.cmake      # Windows 交叉编译
│   │   └── macos_arm64.cmake         # macOS 交叉编译
│   ├── build_*/                      # 🏗️ 各平台构建目录
│   ├── release_v2.0.0/               # 📦 发布包目录
│   └── *.tar.gz, *.zip              # 📁 压缩发布包
├── 🔧 source_linux_armv7l/           # 🎯 嵌入式 ARM 平台源码
│   ├── v4l2_usb.c                    # 🔥 网络推流服务器 (嵌入式端)
│   ├── v4l2_bench.c                  # 📊 单平面基准测试
│   ├── v4l2_bench_mp.c               # 📈 多平面格式基准测试  
│   ├── test_multiplanar.c            # 🧪 多平面 API 验证程序
│   ├── CMakeLists.txt                # 🔨 ARM 构建配置
│   ├── create_release.sh             # 📦 嵌入式端发布脚本
│   ├── build/                        # 🏗️ ARM 构建产物
│   │   ├── v4l2_usb                  # 主服务器程序
│   │   ├── v4l2_bench                # 性能测试
│   │   ├── v4l2_bench_mp             # 多平面测试
│   │   └── test_multiplanar          # API 验证
│   ├── release_embedded_v2.0.0/      # 📦 嵌入式发布包
│   └── *.tar.gz, *.zip              # 📁 ARM 压缩包
├── CMakeLists.txt                    # 🔨 根级 CMake 配置
├── build.sh                          # 🚀 传统构建脚本 (兼容)
└── README.md                         # 📖 本文档
```

## 🚀 快速开始

### 1. 系统架构

```
┌─────────────────┐    网络推流     ┌─────────────────┐
│   Luckfox Pico  │ ────────────► │   PC 客户端      │
│                 │   TCP:8888    │                 │
│ • v4l2_usb      │               │ • v4l2_usb_pc   │
│ • SC3336 摄像头 │               │ • 帧数据保存     │
│ • 172.32.0.93   │               │ • 实时显示       │
└─────────────────┘               └─────────────────┘
```

### 2. 网络配置

#### 🖥️ PC 端编译 (全平台支持)

```bash
cd source_all_platform

# 一键构建所有平台 (推荐)
./build_all_platforms.sh

# 或分别构建
mkdir build_native && cd build_native
cmake .. && make                                    # Linux 本地

mkdir build_windows && cd build_windows  
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/windows_x86_64.cmake .. && make  # Windows

mkdir build_macos && cd build_macos
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/macos_arm64.cmake .. && make     # macOS

# 生成发布包
./create_release.sh
```

#### 🔧 嵌入式端编译 (ARM)

```bash
cd source_linux_armv7l

# 方式1: 使用 CMake (推荐)
mkdir -p build && cd build
cmake ..
make

# 方式2: 使用传统脚本 (兼容)
cd ../.. && ./build.sh

# 生成嵌入式发布包
cd source_linux_armv7l
./create_release.sh

# 编译结果:
# build/v4l2_usb          - 🔥 网络推流服务器
# build/v4l2_bench        - 📊 单平面基准测试
# build/v4l2_bench_mp     - 📈 多平面基准测试程序
# build/test_multiplanar  - 🧪 多平面验证程序
```

### 4. 网络配置

#### 📤 部署到 Luckfox Pico (如果自行编译)

```bash
# 上传程序到设备  
scp source_linux_armv7l/build/v4l2_usb root@172.32.0.93:/root/
scp source_linux_armv7l/build/v4l2_bench_mp root@172.32.0.93:/root/
```

#### 🎬 启动系统

**步骤1: 在 Luckfox Pico 上启动服务器**
```bash
# SSH 登录到 Luckfox Pico
ssh root@172.32.0.93

# 可选: 先测试摄像头工作状态
./v4l2_bench_mp

# 启动网络推流服务器
./v4l2_usb
# 或指定端口: ./v4l2_usb 8888
```

**步骤2: 在 PC 上启动客户端**

使用预编译版本 (推荐):
```bash
# Linux/macOS
cd release_v2.0.0/linux_x86_64    # 或 macos_arm64  
./run.sh -s 172.32.0.93 -c

# Windows
cd release_v2.0.0/windows_x86_64
run.bat -s 172.32.0.93 -c
```

使用自编译版本:
```bash
# Linux
cd source_all_platform/build_native
./v4l2_usb_pc -s 172.32.0.93 -p 8888 -o ./saved_frames -c

# Windows  
cd source_all_platform/build_windows_x86_64
./v4l2_usb_pc.exe -s 172.32.0.93 -p 8888 -o ./saved_frames -c
```

## 📊 性能指标

| 参数 | 数值 | 说明 |
|------|------|------|
| **分辨率** | 2304×1296 | SC3336 摄像头最大分辨率 |
| **格式** | RAW10 (SBGGR10) | 10位原始拜耳格式 |
| **帧率** | ~30 FPS | 实际帧率取决于网络带宽 |
| **数据量** | ~6.0MB/帧 | 2304×1296×10bit/8 + 对齐 |
| **网络带宽** | ~120MB/s | 建议千兆以太网 |
| **内存占用** | <50MB | 3个缓冲区 + 网络缓存 |

## 🛠️ 使用场景

### 场景1: 快速开始 (预编译版本)
```bash
# 嵌入式端
cd luckfox_pico_armv7l && ./run_v4l2_usb.sh

# PC 端 (Linux)
cd linux_x86_64 && ./run.sh

# PC 端 (Windows) 
cd windows_x86_64 && run.bat
```

### 场景2: 指定保存目录
```bash
# 修改启动脚本或使用自编译版本
./v4l2_usb_pc -o /path/to/save/frames -c
```

### 场景3: 自定义网络配置
```bash
# 如果 Luckfox Pico 使用不同 IP
./v4l2_usb_pc -s 192.168.1.100 -p 8888 -c
```

### 场景4: 多客户端接收
```bash
# 可同时启动多个客户端连接同一服务器
./v4l2_usb_pc -o ./client1_frames -c &
./v4l2_usb_pc -o ./client2_frames -c &
```

### 场景5: 性能测试
```bash
# 嵌入式端性能基准测试
cd luckfox_pico_armv7l && ./run_benchmark.sh

# 多平面 API 测试
./run_multiplanar_test.sh
```

## 🔧 命令行参数

### v4l2_usb (服务器端)
```bash
./v4l2_usb [port]
```
- `port`: 监听端口 (默认: 8888)

### v4l2_usb_pc (客户端)
```bash
./v4l2_usb_pc [-s server_ip] [-p port] [-o output_dir] [-c] [-i interval] [-h]
```
- `-s server_ip`: 服务器IP地址 (默认: 172.32.0.93)
- `-p port`: 服务器端口 (默认: 8888)
- `-o output_dir`: 帧数据保存目录 (默认: ./received_frames)
- `-c, --convert`: 启用 SBGGR10 到 16-bit 转换
- `-i interval`: 保存间隔，每N帧保存一次 (默认: 1)
- `-h, --help`: 显示帮助信息

## 🎁 发布包说明

### PC 端发布包结构
```
v4l2_usb_pc_v2.0.0_all_platforms/
├── linux_x86_64/               # Linux x86_64 版本
│   ├── bin/v4l2_usb_pc         # 可执行文件
│   ├── lib/                    # 依赖库
│   └── run.sh                  # 启动脚本
├── windows_x86_64/             # Windows x86_64 版本  
│   ├── bin/v4l2_usb_pc.exe     # 可执行文件
│   ├── lib/                    # 依赖库
│   └── run.bat                 # 启动脚本
├── macos_arm64/                # macOS ARM64 版本
│   ├── bin/v4l2_usb_pc         # 可执行文件
│   ├── lib/                    # 依赖库
│   └── run.sh                  # 启动脚本
├── README.md                   # 详细说明
└── USAGE.txt                   # 快速入门
```

### 嵌入式发布包结构
```
luckfox_pico_armv7l/
├── bin/                        # ARM 可执行文件
│   ├── v4l2_usb               # 🔥 主服务器
│   ├── v4l2_bench             # 📊 性能测试
│   ├── v4l2_bench_mp          # 📈 多平面测试  
│   └── test_multiplanar       # 🧪 API 验证
├── config/                     # 配置文件
│   ├── device.conf            # 设备配置
│   └── network.conf           # 网络配置
├── scripts/                    # 系统脚本
│   ├── install.sh             # 自动安装
│   └── uninstall.sh           # 自动卸载
├── run_v4l2_usb.sh            # 🚀 主程序启动
├── run_benchmark.sh           # 📊 性能测试启动
├── run_multiplanar_test.sh    # 🧪 API 测试启动
└── *.c                        # 源代码 (便于定制)
```

## ❓ 常见问题

### Q1: 连接失败 "Connection refused"
**原因**: 网络不通或服务器未启动
**解决**:
1. 检查网络连接: `ping 172.32.0.93`
2. 确认服务器已启动: 在 Luckfox Pico 上运行 `./v4l2_usb`
3. 检查防火墙设置

### Q2: 帧率很低或卡顿
**原因**: 网络带宽不足
**解决**:
1. 使用千兆以太网
2. 减少网络负载
3. 检查网络质量: `iperf3` 测速

### Q3: "No such file or directory" 错误
**原因**: 摄像头设备未找到
**解决**:
1. 检查摄像头连接: `ls /dev/video*`
2. 确认驱动加载: `dmesg | grep -i video`
3. 重新插拔摄像头

### Q4: 编译错误
**解决**:
```bash
# 确保依赖库已安装
sudo apt update
sudo apt install build-essential cmake

# 清理重新编译
rm -rf build
mkdir build && cd build
cmake ..
make
```

### Q5: Windows 编译问题
**解决**:
1. 安装 MinGW-w64
2. 确保 `gcc` 在 PATH 中
3. 使用 `build_windows.bat` 自动编译

### Q6: 程序无法正常退出
**解决**: 使用 `Ctrl+C` 发送 SIGINT 信号，程序会优雅退出并清理资源

## 🔍 调试与监控

### 查看运行状态
```bash
# 服务器端会显示:
# - 客户端连接状态
# - 实时帧率统计
# - 网络传输速度

# 客户端会显示:
# - 接收帧数统计
# - 保存文件路径
# - 传输速度
```

### 性能监控
```bash
# 网络流量监控
iftop -i eth0

# 系统资源监控
htop

# 磁盘使用监控 (如果保存文件)
df -h
```

## ⚙️ 构建系统说明

### 自动化构建流程
```bash
# PC 端: 构建所有平台 → 打包发布
cd source_all_platform
./build_all_platforms.sh    # 构建 Linux/Windows/macOS
./create_release.sh         # 打包所有平台

# 嵌入式端: 构建 ARM → 打包发布  
cd source_linux_armv7l
mkdir build && cd build && cmake .. && make  # 构建 ARM
cd .. && ./create_release.sh                 # 打包嵌入式
```

### CMake 工具链说明
- `toolchains/windows_x86_64.cmake`: Windows 交叉编译
- `toolchains/macos_arm64.cmake`: macOS ARM64 交叉编译
- 原生平台直接使用系统默认工具链

### 发布包自动化
- **版本控制**: 统一版本号管理 (v2.0.0)
- **平台识别**: 自动检测并生成对应平台包
- **压缩格式**: Windows 用 ZIP，其他用 TAR.GZ
- **文档生成**: 自动生成 README.md 和 USAGE.txt

## 📈 技术细节

### V4L2 多平面 API
- 使用 `VIDIOC_S_FMT` 设置多平面格式
- 256字节内存对齐优化
- 零拷贝内存映射

### 网络传输优化
- TCP 协议确保数据完整性
- 多线程异步传输
- 客户端断开自动重连

### 信号处理
- 完善的 SIGINT/SIGTERM 处理
- 线程安全的退出机制
- 资源自动清理

## 🤝 贡献与支持

如有问题或建议，请提交 Issue 或 Pull Request。

### 开发环境
- **嵌入式**: Luckfox Pico SDK + GCC ARM 交叉编译器
- **PC 跨平台**: CMake + 多工具链支持
  - Linux: GCC
  - Windows: MinGW-w64 交叉编译  
  - macOS: Clang (Apple Silicon)

## 🚀 版本历史

### v2.0.0 (当前版本)
- ✅ **全面重构**: 代码模块化，易于维护
- ✅ **跨平台支持**: Windows、Linux、macOS、ARM Linux
- ✅ **自动化构建**: 一键编译、打包、发布
- ✅ **即开即用**: 预编译二进制文件
- ✅ **增强功能**: SBGGR10转换、帧间隔控制
- ✅ **完善文档**: 详细使用说明和故障排除

### v1.0.0 (传统版本)
- ✅ 基本的 V4L2 图像采集
- ✅ TCP 网络传输
- ✅ 多平面 API 支持

---

**📝 版本**: v2.0.0  
**🏷️ 标签**: V4L2, RAW10, 网络推流, 嵌入式, 跨平台, 高性能  
**📅 更新**: 2025年6月最新版本  
**🔗 架构**: 模块化 • 自动化 • 即开即用
