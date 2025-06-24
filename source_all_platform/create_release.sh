#!/bin/bash

# create_release.sh - 创建发布包脚本
# 将构建的二进制文件打包为发布版本

set -e

# 颜色定义
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

VERSION="2.0.0"
RELEASE_DIR="release_v${VERSION}"

log_info "Creating release package v${VERSION}"

# 清理并创建发布目录
rm -rf "${RELEASE_DIR}"
mkdir -p "${RELEASE_DIR}"

# 复制Linux版本
if [ -d "build_native/dist/linux_x86_64" ]; then
    log_info "Packaging Linux x86_64..."
    mkdir -p "${RELEASE_DIR}/linux_x86_64"
    cp -r build_native/dist/linux_x86_64/* "${RELEASE_DIR}/linux_x86_64/"
    
    # 创建Linux启动脚本
    cat > "${RELEASE_DIR}/linux_x86_64/run.sh" << 'EOF'
#!/bin/bash
cd "$(dirname "$0")"
./bin/v4l2_usb_pc "$@"
EOF
    chmod +x "${RELEASE_DIR}/linux_x86_64/run.sh"
fi

# 复制Windows版本
if [ -d "build_windows_x86_64/dist/windows_x86_64" ]; then
    log_info "Packaging Windows x86_64..."
    mkdir -p "${RELEASE_DIR}/windows_x86_64"
    cp -r build_windows_x86_64/dist/windows_x86_64/* "${RELEASE_DIR}/windows_x86_64/"
    
    # 创建Windows批处理文件
    cat > "${RELEASE_DIR}/windows_x86_64/run.bat" << 'EOF'
@echo off
cd /d "%~dp0"
bin\v4l2_usb_pc.exe %*
pause
EOF
fi

# 复制macOS版本（如果存在）
if [ -d "build_macos_arm64/dist/macos_arm64" ]; then
    log_info "Packaging macOS ARM64..."
    mkdir -p "${RELEASE_DIR}/macos_arm64"
    cp -r build_macos_arm64/dist/macos_arm64/* "${RELEASE_DIR}/macos_arm64/"
    
    # 创建macOS启动脚本
    cat > "${RELEASE_DIR}/macos_arm64/run.sh" << 'EOF'
#!/bin/bash
cd "$(dirname "$0")"
./bin/v4l2_usb_pc "$@"
EOF
    chmod +x "${RELEASE_DIR}/macos_arm64/run.sh"
fi

# 复制文档和源码
log_info "Copying documentation and source code..."
cp README.md "${RELEASE_DIR}/README.md"
cp v4l2_usb_pc.h "${RELEASE_DIR}/"
cp CMakeLists.txt "${RELEASE_DIR}/"

# 创建总体使用说明
cat > "${RELEASE_DIR}/USAGE.txt" << EOF
V4L2 USB PC Client v${VERSION} - Release Package
==============================================

This package contains cross-platform binaries for:
- Linux x86_64
- Windows x86_64
- macOS ARM64 (if available)

Quick Start:
-----------

Linux:
  cd linux_x86_64
  ./run.sh --help
  ./run.sh -s 172.32.0.93 -c

Windows:
  cd windows_x86_64
  run.bat --help
  run.bat -s 172.32.0.93 -c

macOS:
  cd macos_arm64
  ./run.sh --help
  ./run.sh -s 172.32.0.93 -c

Command Line Options:
--------------------
  -s, --server IP     Server IP address (default: 172.32.0.93)
  -p, --port PORT     Server port (default: 8888)
  -o, --output DIR    Output directory (default: ./received_frames)
  -c, --convert       Enable SBGGR10 to 16-bit conversion
  -i, --interval N    Save every Nth frame (default: 1)
  -h, --help          Show help message

Files Included:
--------------
  bin/                Executable files
  lib/                Static and dynamic libraries
  run.*               Platform-specific launch scripts
  README.md           Detailed documentation
  v4l2_usb_pc.h       Header file for API integration
  CMakeLists.txt      Build configuration

Build Information:
-----------------
  Version: ${VERSION}
  Build Date: $(date)
  Features: Multi-threading, SIMD optimization, Cross-platform
  
Support:
-------
  For issues and updates, please check the project repository.
EOF

# 创建发布压缩包
log_info "Creating release archives..."

for platform_dir in "${RELEASE_DIR}"/*/; do
    if [ -d "${platform_dir}" ]; then
        platform_name=$(basename "${platform_dir}")
        archive_name="v4l2_usb_pc_v${VERSION}_${platform_name}"
        
        cd "${RELEASE_DIR}"
        if [[ "${platform_name}" == *"windows"* ]]; then
            zip -r "../${archive_name}.zip" "${platform_name}/" README.md USAGE.txt
            log_success "Created ${archive_name}.zip"
        else
            tar -czf "../${archive_name}.tar.gz" "${platform_name}/" README.md USAGE.txt
            log_success "Created ${archive_name}.tar.gz"
        fi
        cd ..
    fi
done

# 创建完整发布包
cd "${RELEASE_DIR}"
zip -r "../v4l2_usb_pc_v${VERSION}_all_platforms.zip" .
tar -czf "../v4l2_usb_pc_v${VERSION}_all_platforms.tar.gz" .
cd ..

log_success "Release package created: ${RELEASE_DIR}/"
log_info "Archive files:"
ls -lh v4l2_usb_pc_v${VERSION}_*.{zip,tar.gz} 2>/dev/null || true

log_info "Release package contents:"
find "${RELEASE_DIR}" -type f | sort
