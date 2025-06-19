# V4L2 USB流媒体传输系统

这是一个完整的视频流传输系统，包含嵌入式端（Luckfox Pico Mini B）和PC端接收程序。

## 系统架构

```
Luckfox Pico Mini B (172.32.0.100:8888) → PC接收端 (172.32.0.100)
        ↓                                        ↓
    v4l2_usb                                v4l2_usb_pc
    (TCP服务器)                              (TCP客户端)
```

## 网络配置

**重要**: Luckfox Pico Mini B 通常使用特定的网络配置：
- 嵌入式端绑定IP: `172.32.0.100`
- 端口: `8888`
- PC端连接IP: `172.32.0.100`

## 编译

### 嵌入式端 (在开发主机上交叉编译)
```bash
# 使用CMake构建系统
cd /path/to/v4l2_bench
cmake --build build
# 或者单独编译
make -C build v4l2_usb
```

### PC端 (本地编译)
```bash
cd source_pc
make
# 或者使用gcc直接编译
gcc -o v4l2_usb_pc v4l2_usb_pc.c -lm
```

## 使用方法

### 1. 部署嵌入式端程序
```bash
# 上传到设备
scp build/v4l2_usb root@172.32.0.100:/root/

# 在设备上运行
ssh root@172.32.0.100
cd /root
./v4l2_usb
```

### 2. 运行PC端接收程序
```bash
# 基本用法（使用默认配置172.32.0.100）
./v4l2_usb_pc

# 指定服务器IP和端口
./v4l2_usb_pc --server 172.32.0.100 --port 8888

# 指定输出目录
./v4l2_usb_pc --output ./my_frames

# 完整命令示例
./v4l2_usb_pc --server 172.32.0.100 --port 8888 --output ./received_frames
```

### 3. 命令行参数

PC端程序支持以下参数：
- `-s, --server <IP>`  : 服务器IP地址 (默认: 172.32.0.100)
- `-p, --port <PORT>`  : 服务器端口 (默认: 8888)  
- `-o, --output <DIR>` : 输出目录 (默认: ./received_frames)
- `-h, --help`         : 显示帮助信息

## 数据格式

传输的RAW图像格式：
- 分辨率: 2048x1296
- 像素格式: SBGGR10 (10-bit Bayer)
- 文件大小: ~3.17MB 每帧
- 文件扩展名: .BG10

## 网络要求

- 网络带宽: 建议至少100Mbps (每帧3.17MB × 30FPS ≈ 95MB/s)
- 延迟: 低延迟网络连接
- 协议: TCP (可靠传输)

## 故障排除

### 1. 连接问题
```bash
# 检查网络连通性
ping 172.32.0.100

# 检查端口是否开放
telnet 172.32.0.100 8888
```

### 2. 嵌入式端问题
```bash
# 检查设备能力
v4l2-ctl -d /dev/video0 --info

# 检查网络接口
ip addr show

# 检查进程状态
ps aux | grep v4l2_usb
```

### 3. PC端问题
```bash
# 检查输出目录权限
ls -la received_frames/

# 查看接收统计
# (程序运行时会显示实时统计信息)
```

## 性能优化

1. **网络配置**:
   - 使用有线连接而非WiFi
   - 检查网络MTU设置
   - 关闭不必要的网络服务

2. **存储配置**:
   - 使用SSD而非机械硬盘存储接收的文件
   - 确保有足够的磁盘空间

3. **系统配置**:
   - 关闭省电模式
   - 调整TCP缓冲区大小

## 示例输出

```
V4L2 USB Frame Receiver
=======================
Server IP: 172.32.0.100
Port: 8888
Output directory: ./received_frames

Connecting to 172.32.0.100:8888...
Connected successfully!
Created output directory: ./received_frames

Frame 0001: 3317760 bytes saved to ./received_frames/frame_0001.BG10
Frame 0002: 3317760 bytes saved to ./received_frames/frame_0002.BG10
...

=== Statistics ===
Frames received: 100
Total bytes: 331776000 (316.36 MB)
Duration: 3.12 seconds
Average FPS: 32.05
```

## 重要注意事项

1. **网络配置**: 确保Luckfox Pico Mini B配置为172.32.0.100网段
2. **防火墙**: 确保8888端口未被防火墙阻止
3. **存储空间**: 每帧约3.17MB，确保有足够的存储空间
4. **实时性**: 对于实时应用，建议使用低延迟网络连接
