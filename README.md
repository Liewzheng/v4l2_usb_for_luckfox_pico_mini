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
│   ├── verify_unpacked.py            # � 16-bit 解包验证工具
│   └── README.md                     # � PC 端说明文档
├── 🔧 source_linux_armv7l/           # 🎯 嵌入式 ARM 平台源码
│   ├── v4l2_usb.c                    # 🔥 网络推流服务器 (嵌入式端)
│   ├── v4l2_bench.c                  # 📊 单平面基准测试
│   ├── v4l2_bench_mp.c               # 📈 多平面格式基准测试  
│   ├── test_multiplanar.c            # 🧪 多平面 API 验证程序
│   ├── CMakeLists.txt                # 🔨 ARM 构建配置
│   └── create_release.sh             # 📦 嵌入式端发布脚本
├── 🎨 source_python_gui/             # 🖼️ Python GUI 客户端
│   ├── main.py                       # 🖥️ PySide6 GUI 主程序
│   ├── test_client.py                # 🧪 C 库接口测试程序
│   ├── requirements.txt              # 📋 Python 依赖列表
│   └── libv4l2_usb_pc.dll           # 🔗 动态库 (自动生成)
├── build.sh                          # � 传统构建脚本 (兼容)
├── .gitignore                        # � Git 忽略文件配置
├── .clang-format                     # 🎨 代码格式化配置
└── README.md                         # 📖 本文档
```

## 🚀 快速开始

### 1. 系统架构

```
┌─────────────────┐    网络推流     ┌─────────────────────────┐
│   Luckfox Pico  │ ────────────► │   PC 客户端              │
│                 │   TCP:8888    │                         │
│ • v4l2_usb      │               │ • v4l2_usb_pc (命令行)   │
│ • SC3336 摄像头 │               │ • Python GUI (图形界面)  │
│ • 172.32.0.93   │               │ • 16-bit RAW 图像显示    │
└─────────────────┘               └─────────────────────────┘
```

### 2. 三种使用方式

#### 🎨 方式1: Python GUI (推荐新用户)
```bash
cd source_python_gui

# 安装 Python 依赖
pip install -r requirements.txt

# 构建 C 动态库
cd ../source_all_platform
mkdir -p build && cd build
cmake -DBUILD_SHARED_LIBS=ON .. && make
cp libv4l2_usb_pc.so ../source_python_gui/  # Linux
# 或 cp libv4l2_usb_pc.dll ../source_python_gui/  # Windows

# 启动 GUI
cd ../../source_python_gui
python main.py
```

#### ⚡ 方式2: 预编译版本 (即开即用)
```bash
# 下载发布包
# � [PC 端多平台包](source_all_platform/v4l2_usb_pc_v2.0.0_all_platforms.zip)
# 📥 [嵌入式 ARM 包](source_linux_armv7l/v4l2_usb_embedded_v2.0.0_luckfox_pico_armv7l.tar.gz)

# PC 端使用
tar -xf v4l2_usb_pc_v2.0.0_linux_x86_64.tar.gz    # Linux
cd linux_x86_64 && ./run.sh                       # 启动

# 嵌入式端部署
scp v4l2_usb_embedded_v2.0.0_luckfox_pico_armv7l.tar.gz root@172.32.0.93:~/
ssh root@172.32.0.93
tar -xf v4l2_usb_embedded_v2.0.0_luckfox_pico_armv7l.tar.gz
cd luckfox_pico_armv7l && ./run_v4l2_usb.sh
```

#### 🔨 方式3: 自定义编译 (开发者)

**PC 端编译**:
```bash
cd source_all_platform

# 构建命令行版本
mkdir -p build && cd build
cmake .. && make

# 构建动态库 (给 Python GUI 使用)
mkdir -p build_shared && cd build_shared
cmake -DBUILD_SHARED_LIBS=ON .. && make

# 生成发布包
cd .. && ./create_release.sh
```

**嵌入式端编译**:

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

### 场景1: 图形界面实时查看 (Python GUI)
```bash
# 启动 Python GUI
cd source_python_gui
python main.py

# GUI 功能:
# • 16-bit RAW 图像实时显示 (类似 ImageJ)
# • 自动对比度调整 (1%-99% 分位数拉伸)
# • 灰度/伪彩色模式切换
# • 图像统计信息显示 (Min/Max/Mean/Std)
# • 保存模式选择 (仅内存/保存到文件)
# • 实时帧率和连接状态监控
```

### 场景2: 命令行批量处理
```bash
# 命令行版本 - 适合脚本自动化
./v4l2_usb_pc -s 172.32.0.93 -o ./saved_frames -c -i 5
# -c: 启用 SBGGR10 到 16-bit 转换
# -i 5: 每 5 帧保存一次
```

### 场景3: 自定义网络配置
```bash
# 如果 Luckfox Pico 使用不同 IP
# GUI 方式: 在界面中修改服务器 IP
# 命令行方式:
./v4l2_usb_pc -s 192.168.1.100 -p 8888 -c
```

### 场景4: 多客户端同时接收
```bash
# 可同时启动多个客户端连接同一服务器
# 方式1: 多个 GUI 实例
python main.py &  # GUI 客户端1
python main.py &  # GUI 客户端2

# 方式2: GUI + 命令行混合
python main.py &                           # GUI 实时查看
./v4l2_usb_pc -o ./batch_save -c -i 10 &  # 后台批量保存
```

### 场景5: 图像质量分析
```bash
# 使用验证工具检查 16-bit 转换质量
cd source_all_platform
python verify_unpacked.py [16bit_file.raw]

# GUI 中查看实时统计:
# • 像素值范围 (Min/Max)
# • 图像亮度 (Mean)
# • 对比度 (Std)
```

### 场景6: 性能基准测试
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

### v4l2_usb_pc (命令行客户端)
```bash
./v4l2_usb_pc [-s server_ip] [-p port] [-S save_path] [-c] [-i interval] [-h]
```
- `-s server_ip`: 服务器IP地址 (默认: 172.32.0.93)
- `-p port`: 服务器端口 (默认: 8888)
- `-S save_path`: 帧数据保存目录 (默认: 仅内存模式)
- `-c, --convert`: 启用 SBGGR10 到 16-bit 转换
- `-i interval`: 保存间隔，每N帧保存一次 (默认: 1)
- `-h, --help`: 显示帮助信息

### Python GUI (图形界面客户端)
```bash
cd source_python_gui
python main.py
```
**GUI 功能特性**:
- 🖼️ **实时图像显示**: 16-bit RAW 图像，类似 ImageJ 体验
- 📊 **图像分析**: 自动对比度、统计信息 (Min/Max/Mean/Std)
- 🎨 **显示模式**: 灰度/伪彩色切换
- 💾 **保存控制**: 仅内存模式 / 文件保存模式
- 📈 **性能监控**: 实时帧率、连接状态、传输速度
- ⚙️ **参数配置**: 服务器 IP/端口、保存路径设置

**GUI 操作方式**:
1. 在界面中设置服务器 IP 和端口
2. 选择保存模式 (仅内存 或 指定保存路径)
3. 点击"开始接收"连接服务器
4. 实时查看 16-bit RAW 图像和统计信息
5. 使用"停止接收"断开连接

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

### Q1: Python GUI 启动失败
**原因**: Python 依赖未安装或 C 动态库缺失
**解决**:
```bash
# 安装 Python 依赖
cd source_python_gui
pip install -r requirements.txt

# 构建 C 动态库
cd ../source_all_platform
mkdir -p build && cd build
cmake -DBUILD_SHARED_LIBS=ON .. && make

# 复制动态库到 GUI 目录
cp libv4l2_usb_pc.so ../source_python_gui/    # Linux
# 或 cp libv4l2_usb_pc.dll ../source_python_gui/  # Windows
```

### Q2: GUI 显示黑屏或模拟图像
**原因**: C 库未正确获取到实际图像数据
**解决**:
1. 确认嵌入式端正在推流: 检查服务器输出
2. 检查网络连接状态: GUI 中查看连接状态
3. 确认 SBGGR10 转换已启用: C 库会自动转换为 16-bit
4. 重启 GUI 程序重新连接

### Q3: 16-bit 图像显示异常
**原因**: 图像数据范围或格式问题
**解决**:
```bash
# 使用验证工具检查数据
cd source_all_platform
python verify_unpacked.py [16bit_file.raw]

# 在 GUI 中查看图像统计信息
# 检查 Min/Max/Mean/Std 值是否合理
```

### Q4: 连接失败 "Connection refused"
**原因**: 网络不通或服务器未启动
**解决**:
1. 检查网络连接: `ping 172.32.0.93`
2. 确认服务器已启动: 在 Luckfox Pico 上运行 `./v4l2_usb`
3. 检查防火墙设置

### Q5: 帧率很低或卡顿
**原因**: 网络带宽不足
**解决**:
1. 使用千兆以太网
2. 减少网络负载
3. 检查网络质量: `iperf3` 测速

### Q6: "No such file or directory" 错误
**原因**: 摄像头设备未找到
**解决**:
1. 检查摄像头连接: `ls /dev/video*`
2. 确认驱动加载: `dmesg | grep -i video`
3. 重新插拔摄像头

### Q7: 编译错误
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

### Q8: Windows 编译问题
**解决**:
1. 安装 MinGW-w64
2. 确保 `gcc` 在 PATH 中
3. 使用 `build_windows.bat` 自动编译

### Q9: 程序无法正常退出
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
- ✅ **Python GUI**: PySide6 图形界面，16-bit RAW 图像实时显示
- ✅ **图像处理**: SBGGR10→16bit 转换，自动对比度，伪彩色映射
- ✅ **性能优化**: 多线程解包，SIMD 加速，内存池管理
- ✅ **自动化构建**: 一键编译、打包、发布
- ✅ **即开即用**: 预编译二进制文件
- ✅ **双模式运行**: 命令行批处理 + GUI 实时查看
- ✅ **完善文档**: 详细使用说明和故障排除

### v1.0.0 (传统版本)
- ✅ 基本的 V4L2 图像采集
- ✅ TCP 网络传输
- ✅ 多平面 API 支持

---

**📝 版本**: v2.0.0  
**🏷️ 标签**: V4L2, RAW10, 网络推流, 嵌入式, 跨平台, Python GUI, 16-bit 图像处理  
**📅 更新**: 2025年6月最新版本  
**🔗 架构**: 模块化 • 自动化 • 即开即用 • 图形界面
