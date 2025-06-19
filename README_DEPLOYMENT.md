# 完整部署说明

## 系统概述

本系统包含两个部分：
1. **Luckfox Pico Mini B** (嵌入式设备端) - 运行 `v4l2_usb` 服务器
2. **PC端** (Windows/Linux) - 运行 `v4l2_usb_pc` 客户端

## 网络配置

### 默认 IP 配置
- **Luckfox Pico**: `172.32.0.93:8888` (服务器)
- **PC客户端**: 连接到 `172.32.0.93:8888`

### 网络要求
- 确保 PC 和 Luckfox Pico 在同一网络段
- 建议使用千兆以太网（数据率约100MB/s）
- 防火墙允许端口 8888 通信

## 部署步骤

### 1. 编译程序

#### 嵌入式端 (在开发PC上交叉编译)
```bash
cd /home/liewzheng/Workspace/luckfox_pico_app/v4l2_bench
cmake --build build
# 或单独编译
make -C build v4l2_usb v4l2_bench_mp
```

#### PC端 - Linux
```bash
make -f Makefile.pc
```

#### PC端 - Windows
```cmd
build_windows.bat
# 或
mingw32-make -f Makefile.win
# 或手动
gcc -o v4l2_usb_pc.exe v4l2_usb_pc_win.c -lws2_32
```

### 2. 部署到设备

#### 上传到 Luckfox Pico
```bash
scp build/v4l2_usb build/v4l2_bench_mp root@172.32.0.93:/root/Workspace/
```

### 3. 运行系统

#### 在 Luckfox Pico 上启动服务器
```bash
# 先测试摄像头
./v4l2_bench_mp

# 启动流式传输服务器
./v4l2_usb
# 或指定端口
./v4l2_usb 8888
```

#### 在 PC 上启动客户端

**Linux:**
```bash
./v4l2_usb_pc
# 或指定参数
./v4l2_usb_pc -s 172.32.0.93 -p 8888 -o ./frames
```

**Windows:**
```cmd
v4l2_usb_pc.exe
# 或指定参数
v4l2_usb_pc.exe -s 172.32.0.93 -p 8888 -o ./frames
```

## 使用场景

### 场景1: 基本图像采集
```bash
# Luckfox Pico
./v4l2_usb

# PC (另一个终端)
./v4l2_usb_pc
```

### 场景2: 指定输出目录
```bash
# PC
./v4l2_usb_pc -o /path/to/save/frames
```

### 场景3: 不同网络配置
```bash
# 如果 Luckfox Pico IP 是 192.168.1.100
./v4l2_usb_pc -s 192.168.1.100 -p 8888
```

## 输出文件

### 文件格式
- **命名**: `frame_XXXXXX_2048x1296.BG10`
- **大小**: 3,317,760 字节/帧
- **格式**: RAW10 (SBGGR10)
- **保存间隔**: 每10帧保存一次

### 示例输出目录
```
received_frames/
├── frame_000000_2048x1296.BG10  (3.17 MB)
├── frame_000010_2048x1296.BG10  (3.17 MB)
├── frame_000020_2048x1296.BG10  (3.17 MB)
└── ...
```

## 性能指标

### 实测性能
- **帧率**: ~32 FPS
- **分辨率**: 2048x1296
- **数据率**: ~100 MB/s
- **延迟**: < 50ms (典型)

### 系统要求
- **网络**: 千兆以太网推荐
- **存储**: SSD 推荐 (高速写入)
- **内存**: 至少 1GB 可用空间

## 故障排除

### 常见问题及解决方案

#### 1. 连接失败
```
connect failed: Connection refused
```
**解决方案:**
- 检查 Luckfox Pico 是否已启动服务器
- 确认 IP 地址和端口正确
- 检查网络连通性: `ping 172.32.0.93`

#### 2. 权限错误 (Luckfox Pico)
```
VIDIOC_S_FMT failed: Permission denied
```
**解决方案:**
- 使用 root 权限运行: `sudo ./v4l2_usb`
- 检查摄像头设备权限: `ls -la /dev/video*`

#### 3. 内存不足 (PC端)
```
Failed to allocate 3317760 bytes for frame buffer
```
**解决方案:**
- 关闭其他应用释放内存
- 减少保存间隔或停止保存文件

#### 4. 网络超时
```
Timeout waiting for frame
```
**解决方案:**
- 检查网络稳定性
- 增加超时时间
- 使用有线连接

### 调试命令

#### 设备端调试
```bash
# 检查摄像头
ls -la /dev/video*
v4l2-ctl -d /dev/video0 --info

# 检查网络
ifconfig
netstat -tlnp | grep 8888

# 检查进程
ps aux | grep v4l2_usb
```

#### PC端调试
```bash
# Linux
netstat -tn | grep 8888
lsof -i :8888

# Windows
netstat -an | findstr 8888
```

## 高级配置

### 修改 IP 地址

#### 方法1: 编译时修改
修改源代码中的 `DEFAULT_SERVER_IP` 宏定义

#### 方法2: 运行时指定
```bash
# PC端支持命令行参数
./v4l2_usb_pc -s <your_ip> -p <your_port>
```

### 性能优化

#### 网络优化
```bash
# Linux: 增加网络缓冲区
echo 'net.core.rmem_max = 16777216' >> /etc/sysctl.conf
echo 'net.core.wmem_max = 16777216' >> /etc/sysctl.conf
sysctl -p
```

#### 存储优化
```bash
# 使用内存盘 (Linux)
mkdir /tmp/ramdisk
mount -t tmpfs -o size=1G tmpfs /tmp/ramdisk
./v4l2_usb_pc -o /tmp/ramdisk/frames
```

## 扩展功能

### 未来可能的改进
- [ ] 实时预览功能
- [ ] 图像压缩传输
- [ ] 多客户端支持
- [ ] 录像功能
- [ ] 自动曝光控制

---

**开发者**: GitHub Copilot  
**更新时间**: 2025年6月19日
