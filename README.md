# Luckfox Pico V4L2 摄像头高性能采集与网络推流系统

一个专为 **Luckfox Pico Mini B + SC3336 RAW10 摄像头** 设计的高性能视频采集与网络推流系统，支持 2048×1296 分辨率实时采集，并通过网络推流至 PC 端进行处理。

## 🎯 项目特色

- **高分辨率支持**: 2048×1296 RAW10 格式，专业级图像质量
- **高性能传输**: 基于多平面 V4L2 API，优化内存对齐与缓冲区管理
- **跨平台兼容**: 支持 Linux x86_64、Windows 平台
- **网络推流**: TCP 实时推流，支持多客户端连接
- **零临时文件**: 纯内存传输，避免存储空间限制
- **优雅退出**: 完善的信号处理，支持 Ctrl+C 正常退出

## 📁 项目结构

```
v4l2_bench/
├── source_linux_armv7l/          # 嵌入式 ARM 平台源码
│   ├── v4l2_usb.c                 # 🔥 网络推流服务器 (嵌入式端)
│   ├── v4l2_bench_mp.c            # 多平面格式基准测试
│   └── test_multiplanar.c         # 多平面 API 验证程序
├── source_linux_x86_64/          # Linux PC 平台源码
│   └── v4l2_usb_pc.c              # 网络接收客户端 (Linux PC)
├── source_win_x86_64/            # Windows 平台源码
│   ├── v4l2_usb_pc_win.c          # 网络接收客户端 (Windows PC)
│   └── build_windows.bat          # Windows 一键编译脚本
├── CMakeLists.txt                 # CMake 构建配置
├── build.sh                       # 一键编译脚本
└── README.md                      # 📖 本文档
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

| 设备 | IP 地址 | 端口 | 角色 |
|------|---------|------|------|
| **Luckfox Pico** | `172.32.0.93` | `8888` | 服务器 (推流端) |
| **PC 客户端** | 同网段任意IP | - | 客户端 (接收端) |

> **⚠️ 重要**: 确保 PC 和 Luckfox Pico 在同一网络段，建议使用千兆以太网。

### 3. 编译程序

#### 🔧 嵌入式端 (交叉编译)

```bash
# 方式1: 使用 CMake (推荐)
cd v4l2_bench
mkdir -p build && cd build
cmake ..
make

# 方式2: 使用一键脚本
./build.sh

# 编译结果:
# build/v4l2_usb          - 网络推流服务器
# build/v4l2_bench_mp     - 基准测试程序
# build/test_multiplanar  - 多平面验证程序
```

#### 🖥️ Linux PC 端

```bash
cd source_linux_x86_64
make
# 生成: v4l2_usb_pc
```

#### 🪟 Windows PC 端

```cmd
cd source_win_x86_64
build_windows.bat
# 生成: v4l2_usb_pc.exe
```

或手动编译：
```cmd
gcc -o v4l2_usb_pc.exe v4l2_usb_pc_win.c -lws2_32
```

### 4. 部署与运行

#### 📤 部署到 Luckfox Pico

```bash
# 上传程序到设备
scp build/v4l2_usb build/v4l2_bench_mp root@172.32.0.93:/root/
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

Linux:
```bash
./v4l2_usb_pc
# 或自定义参数: ./v4l2_usb_pc -s 172.32.0.93 -p 8888 -o ./saved_frames
```

Windows:
```cmd
v4l2_usb_pc.exe
# 或自定义参数: v4l2_usb_pc.exe -s 172.32.0.93 -p 8888 -o ./saved_frames
```

## 📊 性能指标

| 参数 | 数值 | 说明 |
|------|------|------|
| **分辨率** | 2048×1296 | SC3336 摄像头最大分辨率 |
| **格式** | RAW10 (SBGGR10) | 10位原始拜耳格式 |
| **帧率** | ~30 FPS | 实际帧率取决于网络带宽 |
| **数据量** | ~5.3MB/帧 | 2048×1296×10bit/8 + 256字节对齐 |
| **网络带宽** | ~100MB/s | 建议千兆以太网 |
| **内存占用** | <50MB | 3个缓冲区 + 网络缓存 |

## 🛠️ 使用场景

### 场景1: 基本实时推流
```bash
# Luckfox Pico
./v4l2_usb

# PC (接收并保存到当前目录)
./v4l2_usb_pc
```

### 场景2: 指定保存目录
```bash
# PC
./v4l2_usb_pc -o /path/to/save/frames
```

### 场景3: 自定义网络配置
```bash
# 如果 Luckfox Pico 使用不同 IP
./v4l2_usb_pc -s 192.168.1.100 -p 8888
```

### 场景4: 多客户端接收
```bash
# 可同时启动多个客户端连接同一服务器
./v4l2_usb_pc -o ./client1_frames &
./v4l2_usb_pc -o ./client2_frames &
```

## 🔧 命令行参数

### v4l2_usb (服务器端)
```bash
./v4l2_usb [port]
```
- `port`: 监听端口 (默认: 8888)

### v4l2_usb_pc (客户端)
```bash
./v4l2_usb_pc [-s server_ip] [-p port] [-o output_dir]
```
- `-s server_ip`: 服务器IP地址 (默认: 172.32.0.93)
- `-p port`: 服务器端口 (默认: 8888)
- `-o output_dir`: 帧数据保存目录 (默认: 当前目录)

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
- **Linux**: GCC + CMake
- **Windows**: MinGW-w64 + GCC

---

**📝 版本**: v1.0  
**🏷️ 标签**: V4L2, RAW10, 网络推流, 嵌入式, 跨平台  
**📅 更新**: 2025年最新版本
