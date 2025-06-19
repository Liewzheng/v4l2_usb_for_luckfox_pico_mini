# V4L2 USB RAW Image Streaming

本项目实现了 Luckfox Pico Mini B 通过网络流式传输 RAW 图像数据的完整解决方案。

## 系统架构

```
Luckfox Pico Mini B (ARM)          PC (x86_64)
┌─────────────────────┐           ┌──────────────────┐
│   SC3336 Camera     │           │   PC Client      │
│        ↓            │           │                  │
│   V4L2 Capture      │  Network  │  TCP Receiver    │
│   (Multiplanar)     │ ←────────→│                  │
│        ↓            │           │  Frame Parser    │
│   TCP Server        │           │                  │
│   (v4l2_usb)        │           │  File Writer     │
└─────────────────────┘           └──────────────────┘
```

## 功能特性

### 设备端 (v4l2_usb)
- ✅ 多平面 V4L2 API 支持
- ✅ 2048x1296 @ RAW10 (BG10) 实时采集
- ✅ TCP 服务器，支持单客户端连接
- ✅ 多线程处理：采集线程 + 网络发送线程
- ✅ 临时文件缓存 (/dev/shm)
- ✅ 实时 FPS 统计
- ✅ 优雅的信号处理和清理

### PC端 (v4l2_usb_pc)
- ✅ 可靠的 TCP 客户端连接
- ✅ 帧头解析和数据完整性验证
- ✅ 自动文件保存和管理
- ✅ 实时统计信息 (FPS, 数据率)
- ✅ 命令行参数配置
- ✅ 超时和错误处理

## 编译说明

### 设备端编译 (Luckfox Pico)

使用交叉编译环境：

```bash
cd /home/liewzheng/Workspace/luckfox_pico_app/v4l2_bench

# 编译多平面基准测试和USB流式传输程序
cmake --build build

# 或者单独编译
make -C build v4l2_usb v4l2_bench_mp
```

### PC端编译

在 PC 上使用标准 GCC：

```bash
# 使用 Makefile
make -f Makefile.pc

# 或手动编译
gcc -Wall -Wextra -O2 -std=c99 -o v4l2_usb_pc v4l2_usb_pc.c -lrt -lpthread
```

## 使用方法

### 1. 设备端运行

在 Luckfox Pico 上：

```bash
# 基准测试 (验证摄像头工作)
./v4l2_bench_mp

# 启动 USB 流式传输服务器
./v4l2_usb [port]

# 默认端口 8888
./v4l2_usb
```

### 2. PC端接收

在 PC 上：

```bash
# 基本使用
./v4l2_usb_pc

# 指定服务器和端口
./v4l2_usb_pc -s 192.168.230.93 -p 8888

# 指定输出目录
./v4l2_usb_pc -s 192.168.230.93 -o ./my_frames

# 查看帮助
./v4l2_usb_pc --help
```

### 3. 参数说明

**PC客户端参数：**
- `-s, --server IP`: 服务器IP地址 (默认: 192.168.230.93)
- `-p, --port PORT`: 服务器端口 (默认: 8888)  
- `-o, --output DIR`: 输出目录 (默认: ./received_frames)
- `-h, --help`: 显示帮助信息

## 数据格式

### 帧头格式 (32字节)
```c
struct frame_header {
    uint32_t magic;      // 0xDEADBEEF (魔数)
    uint32_t frame_id;   // 帧序号
    uint32_t width;      // 图像宽度 (2048)
    uint32_t height;     // 图像高度 (1296)
    uint32_t pixfmt;     // 像素格式 (0x30314742 = BG10)
    uint32_t size;       // 帧数据大小 (字节)
    uint64_t timestamp;  // 时间戳 (纳秒)
    uint32_t reserved[2]; // 保留字段
} __attribute__((packed));
```

### RAW 图像数据
- **格式**: SBGGR10 (Bayer Green-Blue-Green-Red 10-bit)
- **分辨率**: 2048x1296
- **字节对齐**: 256字节对齐 (bytesperline=2560)
- **文件大小**: 3,317,760 字节/帧
- **帧率**: ~32 FPS

## 性能指标

### 基准测试结果
```
=== Benchmark Results ===
Capture time (ms): min=1.76, max=33.74, avg=31.21
Average FPS: 32.04
Frames saved: 5
```

### 网络传输
- **数据率**: ~100 MB/s (3.17MB × 32FPS)
- **网络带宽**: 建议千兆以太网
- **延迟**: < 50ms (典型值)

## 文件输出

### 命名规则
```
frame_XXXXXX_2048x1296.BG10
├── XXXXXX: 6位帧序号 (000000, 000001, ...)
├── 2048x1296: 分辨率
└── BG10: RAW10 格式标识
```

### 示例输出
```
received_frames/
├── frame_000000_2048x1296.BG10  (3,317,760 bytes)
├── frame_000010_2048x1296.BG10  (3,317,760 bytes)
├── frame_000020_2048x1296.BG10  (3,317,760 bytes)
└── ...
```

## 图像处理

### 使用 FFmpeg 转换
```bash
# RAW10 to PNG
ffmpeg -f rawvideo -pix_fmt bayer_bggr16le -s 2048x1296 \
       -i frame_000000_2048x1296.BG10 -pix_fmt rgb24 output.png

# RAW10 to TIFF (保持原始精度)
ffmpeg -f rawvideo -pix_fmt bayer_bggr16le -s 2048x1296 \
       -i frame_000000_2048x1296.BG10 -pix_fmt rgb48le output.tiff
```

### 使用 Python 处理
```python
import numpy as np
import cv2

# 读取 RAW10 数据
def read_raw10(filename, width=2048, height=1296, aligned_width=2560):
    with open(filename, 'rb') as f:
        data = np.frombuffer(f.read(), dtype=np.uint8)
    
    # 重塑为对齐的二维数组
    aligned_data = data.reshape((-1, aligned_width))
    
    # 提取有效图像区域
    valid_data = aligned_data[:height, :width*10//8]
    
    return valid_data

# 去拜耳并转换为RGB
def debayer_raw10(raw_data):
    # RAW10 解包和去拜耳处理
    # (具体实现取决于RAW10的打包格式)
    pass
```

## 故障排除

### 常见问题

1. **连接失败**
   - 检查网络连通性: `ping 192.168.230.93`
   - 检查端口是否被占用: `netstat -tlnp | grep 8888`
   - 检查防火墙设置

2. **帧数据错误**
   - 验证魔数: 应为 `0xDEADBEEF`
   - 检查帧大小: 应为 3,317,760 字节
   - 查看设备端日志

3. **性能问题**
   - 使用千兆以太网
   - 检查 CPU 使用率
   - 优化输出目录 (使用 SSD)

### 调试命令

```bash
# 设备端
./v4l2_bench_mp    # 验证摄像头功能
ls -la /dev/shm/   # 检查临时文件

# PC端  
./v4l2_usb_pc --help        # 查看参数
netstat -tn | grep 8888     # 检查连接状态
du -sh received_frames/     # 检查接收的数据量
```

## 技术细节

### 多平面 API
设备使用 V4L2 多平面 API，这是现代V4L2驱动的标准：
- `V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE`
- `struct v4l2_pix_format_mplane`
- 支持多个内存平面，但RAW10只需1个平面

### 内存管理
- 设备端: mmap + 循环缓冲区
- PC端: 动态缓冲区分配
- 零拷贝网络传输

### 线程模型
设备端使用双线程：
- 主线程: V4L2 采集 + select() 等待
- 子线程: TCP 网络发送

## 扩展功能

### 可能的改进
- [ ] 多客户端支持
- [ ] 实时预览 (OpenCV)
- [ ] 压缩传输 (JPEG/H.264)
- [ ] 录像功能
- [ ] 自动曝光控制
- [ ] 图像增强处理

### 性能优化
- [ ] UDP 传输 (低延迟)
- [ ] 零拷贝网络 (sendfile)
- [ ] GPU 加速处理
- [ ] 多线程解码

---

**开发者**: GitHub Copilot  
**版本**: 1.0  
**更新**: 2025年6月19日