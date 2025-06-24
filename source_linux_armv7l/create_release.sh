#!/bin/bash

# create_release.sh - 嵌入式端发布包创建脚本
# 将构建的ARM嵌入式二进制文件打包为发布版本

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

VERSION="2.0.0"
RELEASE_DIR="release_embedded_v${VERSION}"
BUILD_DIR="build"

log_info "Creating embedded release package v${VERSION}"
log_info "Target platform: ARM Linux (armv7l) for Luckfox Pico"

# 检查构建目录是否存在
if [ ! -d "${BUILD_DIR}" ]; then
    log_error "Build directory '${BUILD_DIR}' not found!"
    log_info "Please run 'mkdir build && cd build && cmake .. && make' first"
    exit 1
fi

# 检查可执行文件是否存在
EXECUTABLES=("v4l2_usb" "v4l2_bench" "v4l2_bench_mp" "test_multiplanar")
MISSING_FILES=()

for exe in "${EXECUTABLES[@]}"; do
    if [ ! -f "${BUILD_DIR}/${exe}" ]; then
        MISSING_FILES+=("${exe}")
    fi
done

if [ ${#MISSING_FILES[@]} -gt 0 ]; then
    log_warning "Some executables are missing: ${MISSING_FILES[*]}"
    log_info "Available executables will be packaged"
fi

# 清理并创建发布目录
rm -rf "${RELEASE_DIR}"
mkdir -p "${RELEASE_DIR}"

# 创建嵌入式端目录结构
log_info "Packaging ARM Linux binaries..."
mkdir -p "${RELEASE_DIR}/luckfox_pico_armv7l/bin"
mkdir -p "${RELEASE_DIR}/luckfox_pico_armv7l/scripts"
mkdir -p "${RELEASE_DIR}/luckfox_pico_armv7l/config"

# 复制可执行文件
for exe in "${EXECUTABLES[@]}"; do
    if [ -f "${BUILD_DIR}/${exe}" ]; then
        cp "${BUILD_DIR}/${exe}" "${RELEASE_DIR}/luckfox_pico_armv7l/bin/"
        log_success "Packaged ${exe}"
        
        # 添加可执行权限
        chmod +x "${RELEASE_DIR}/luckfox_pico_armv7l/bin/${exe}"
    fi
done

# 创建嵌入式端启动脚本
cat > "${RELEASE_DIR}/luckfox_pico_armv7l/run_v4l2_usb.sh" << 'EOF'
#!/bin/bash

# V4L2 USB 图像采集启动脚本
# 适用于 Luckfox Pico 嵌入式设备

cd "$(dirname "$0")"

# 默认配置
DEVICE="/dev/video11"
WIDTH="2304"
HEIGHT="1296"
PIXFMT="SBGGR10"
SERVER_IP="172.32.0.93"
SERVER_PORT="8888"

# 检查设备是否存在
if [ ! -e "$DEVICE" ]; then
    echo "Error: V4L2 device $DEVICE not found!"
    echo "Available video devices:"
    ls -la /dev/video* 2>/dev/null || echo "No video devices found"
    exit 1
fi

echo "V4L2 USB Image Capture Server"
echo "============================="
echo "Device: $DEVICE"
echo "Resolution: ${WIDTH}x${HEIGHT}"
echo "Format: $PIXFMT"
echo "Server: ${SERVER_IP}:${SERVER_PORT}"
echo ""

# 启动 V4L2 USB 服务器
echo "Starting V4L2 USB server..."
./bin/v4l2_usb "$@"
EOF

# 创建V4L2基准测试脚本
cat > "${RELEASE_DIR}/luckfox_pico_armv7l/run_benchmark.sh" << 'EOF'
#!/bin/bash

# V4L2 性能基准测试脚本

cd "$(dirname "$0")"

DEVICE="/dev/video11"

echo "V4L2 Performance Benchmark"
echo "=========================="
echo "Device: $DEVICE"
echo ""

if [ ! -e "$DEVICE" ]; then
    echo "Error: V4L2 device $DEVICE not found!"
    exit 1
fi

echo "Running V4L2 benchmark..."
./bin/v4l2_bench "$@"
EOF

# 创建多平面测试脚本
cat > "${RELEASE_DIR}/luckfox_pico_armv7l/run_multiplanar_test.sh" << 'EOF'
#!/bin/bash

# V4L2 多平面格式测试脚本

cd "$(dirname "$0")"

DEVICE="/dev/video11"

echo "V4L2 Multiplanar Test"
echo "===================="
echo "Device: $DEVICE"
echo ""

if [ ! -e "$DEVICE" ]; then
    echo "Error: V4L2 device $DEVICE not found!"
    exit 1
fi

echo "Running multiplanar test..."
./bin/test_multiplanar "$@"
EOF

# 设置脚本可执行权限
chmod +x "${RELEASE_DIR}/luckfox_pico_armv7l/"*.sh

# 创建配置文件
cat > "${RELEASE_DIR}/luckfox_pico_armv7l/config/device.conf" << 'EOF'
# V4L2 设备配置文件
# Device Configuration for V4L2 USB

# 默认视频设备
DEFAULT_DEVICE=/dev/video11

# 默认分辨率
DEFAULT_WIDTH=2304
DEFAULT_HEIGHT=1296

# 默认像素格式
DEFAULT_PIXFMT=SBGGR10

# 网络配置
DEFAULT_SERVER_IP=172.32.0.93
DEFAULT_SERVER_PORT=8888

# 缓冲区配置
BUFFER_COUNT=4
BUFFER_SIZE=auto

# 性能配置
FRAME_RATE=30
CAPTURE_MODE=continuous
EOF

cat > "${RELEASE_DIR}/luckfox_pico_armv7l/config/network.conf" << 'EOF'
# 网络配置文件
# Network Configuration

# TCP服务器配置
TCP_BIND_IP=0.0.0.0
TCP_PORT=8888
TCP_BACKLOG=5

# 数据传输配置
CHUNK_SIZE=65536
SEND_TIMEOUT=10
RECV_TIMEOUT=10

# 连接配置
MAX_CONNECTIONS=1
KEEP_ALIVE=true
EOF

# 复制源码文件
log_info "Copying source code..."
cp *.c "${RELEASE_DIR}/luckfox_pico_armv7l/"
cp CMakeLists.txt "${RELEASE_DIR}/luckfox_pico_armv7l/"

# 创建安装脚本
cat > "${RELEASE_DIR}/luckfox_pico_armv7l/scripts/install.sh" << 'EOF'
#!/bin/bash

# 嵌入式设备安装脚本
# 将二进制文件安装到系统目录

set -e

if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (use sudo)"
    exit 1
fi

echo "Installing V4L2 USB tools to system..."

# 创建目标目录
mkdir -p /usr/local/bin
mkdir -p /etc/v4l2_usb

# 复制可执行文件
cp bin/* /usr/local/bin/
chmod +x /usr/local/bin/v4l2_*
chmod +x /usr/local/bin/test_multiplanar

# 复制配置文件
cp config/* /etc/v4l2_usb/

echo "Installation completed!"
echo "You can now run: v4l2_usb"
EOF

cat > "${RELEASE_DIR}/luckfox_pico_armv7l/scripts/uninstall.sh" << 'EOF'
#!/bin/bash

# 嵌入式设备卸载脚本

set -e

if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (use sudo)"
    exit 1
fi

echo "Uninstalling V4L2 USB tools..."

# 删除可执行文件
rm -f /usr/local/bin/v4l2_usb
rm -f /usr/local/bin/v4l2_bench
rm -f /usr/local/bin/v4l2_bench_mp
rm -f /usr/local/bin/test_multiplanar

# 删除配置目录
rm -rf /etc/v4l2_usb

echo "Uninstallation completed!"
EOF

chmod +x "${RELEASE_DIR}/luckfox_pico_armv7l/scripts/"*.sh

# 创建使用说明文档
cat > "${RELEASE_DIR}/USAGE.txt" << EOF
V4L2 USB Embedded Server v${VERSION} - Release Package
====================================================

This package contains ARM Linux binaries for Luckfox Pico embedded devices.

Target Platform:
---------------
- ARM Linux (armv7l)
- Luckfox Pico Mini B
- Compatible embedded ARM devices

Quick Start:
-----------

1. Copy the entire 'luckfox_pico_armv7l' directory to your embedded device

2. SSH into your device and navigate to the directory:
   cd luckfox_pico_armv7l

3. Make scripts executable (if needed):
   chmod +x *.sh

4. Run the V4L2 USB server:
   ./run_v4l2_usb.sh

5. Or run performance benchmark:
   ./run_benchmark.sh

Available Programs:
------------------
- v4l2_usb          Main USB image capture server
- v4l2_bench        Performance benchmark tool  
- v4l2_bench_mp     Multi-planar benchmark
- test_multiplanar  Multi-planar format test

Command Line Usage:
------------------
Direct execution:
  ./bin/v4l2_usb [options]
  ./bin/v4l2_bench [options]

Configuration Files:
-------------------
- config/device.conf    Device and capture settings
- config/network.conf   Network and server settings

Installation (Optional):
-----------------------
To install system-wide:
  sudo ./scripts/install.sh

To uninstall:
  sudo ./scripts/uninstall.sh

Build Information:
-----------------
  Version: ${VERSION}
  Build Date: $(date)
  Target: ARM Linux armv7l
  Platform: Luckfox Pico
  Features: V4L2 capture, TCP server, Multi-threading
  
Network Setup:
-------------
Default configuration:
- Server IP: 0.0.0.0 (bind all interfaces)
- Server Port: 8888
- Target Resolution: 2304x1296
- Pixel Format: SBGGR10

Make sure your PC client connects to the device's IP address.

Troubleshooting:
---------------
1. Check V4L2 device: ls -la /dev/video*
2. Check permissions: ls -la /dev/video11
3. Check network: ping <device_ip>
4. Check logs: dmesg | grep video

Support:
-------
For issues and updates, please check the project repository.
EOF

# 创建README文件
cat > "${RELEASE_DIR}/README.md" << EOF
# V4L2 USB Embedded Server v${VERSION}

ARM Linux嵌入式图像采集服务器，专为Luckfox Pico设备设计。

## 特性

- 🎥 **V4L2图像采集**: 支持SBGGR10等多种格式
- 🌐 **TCP服务器**: 实时图像数据网络传输  
- ⚡ **高性能**: 多线程优化，低延迟传输
- 🔧 **易于部署**: 预编译ARM二进制文件
- 📊 **性能测试**: 内置基准测试工具

## 快速开始

1. 将整个目录复制到嵌入式设备
2. 运行: \`./run_v4l2_usb.sh\`
3. PC端连接: \`v4l2_usb_pc -s <device_ip>\`

## 文件说明

- \`bin/\`: 可执行文件
- \`config/\`: 配置文件
- \`scripts/\`: 安装/卸载脚本
- \`*.sh\`: 启动脚本

详细使用说明请参考 USAGE.txt 文件。
EOF

# 获取文件大小信息
log_info "Collecting file information..."
cd "${RELEASE_DIR}/luckfox_pico_armv7l"

echo "# File Information" > file_info.txt
echo "Generated on: $(date)" >> file_info.txt
echo "" >> file_info.txt

echo "## Executable Files" >> file_info.txt
for exe in bin/*; do
    if [ -f "$exe" ]; then
        size=$(du -h "$exe" | cut -f1)
        echo "- $(basename $exe): ${size}" >> file_info.txt
    fi
done

echo "" >> file_info.txt
echo "## Total Package Size" >> file_info.txt
total_size=$(du -sh . | cut -f1)
echo "Total: ${total_size}" >> file_info.txt

cd - > /dev/null

# 创建发布压缩包
log_info "Creating release archives..."

archive_name="v4l2_usb_embedded_v${VERSION}_luckfox_pico_armv7l"

cd "${RELEASE_DIR}"
tar -czf "../${archive_name}.tar.gz" .
zip -r "../${archive_name}.zip" . > /dev/null
cd ..

log_success "Release package created: ${RELEASE_DIR}/"
log_info "Archive files:"
ls -lh "${archive_name}".{tar.gz,zip} 2>/dev/null || true

log_info "Package contents:"
find "${RELEASE_DIR}" -type f | sort

echo ""
log_success "Embedded release package v${VERSION} created successfully!"
# log_info "Deploy to device: scp -r ${RELEASE_DIR}/luckfox_pico_armv7l user@device_ip:~/"
# log_info "Then run on device: cd luckfox_pico_armv7l && ./run_v4l2_usb.sh"
