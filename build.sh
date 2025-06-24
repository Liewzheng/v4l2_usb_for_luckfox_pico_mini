#!/bin/bash

# =============================================================================
# Luckfox Pico V4L2 全自动构建脚本 v2.0
# 支持：ARM嵌入式端 + PC跨平台 + Python GUI + 依赖自动安装 + 统一打包
# =============================================================================

set -e  # 遇到错误立即退出

# 颜色输出定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 全局变量
WORK_DIR=$(pwd)
SCRIPT_VERSION="2.0"
BUILD_DATE=$(date +"%Y%m%d_%H%M%S")
FINAL_PACKAGE_NAME="luckfox_v4l2_complete_v2.0_${BUILD_DATE}"

# 日志函数
log_info() {
    echo -e "${CYAN}[INFO]${NC} $1"
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

log_header() {
    echo -e "${PURPLE}======================================${NC}"
    echo -e "${PURPLE}$1${NC}"
    echo -e "${PURPLE}======================================${NC}"
}

# 系统检测函数
detect_system() {
    log_info "检测主机系统..."
    
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        # Linux系统
        if [ -f /etc/os-release ]; then
            . /etc/os-release
            OS="$ID"
            OS_VERSION="$VERSION_ID"
        else
            OS="unknown-linux"
        fi
        ARCH=$(uname -m)
        log_success "检测到 Linux 系统: $OS $OS_VERSION ($ARCH)"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        OS="macos"
        OS_VERSION=$(sw_vers -productVersion)
        ARCH=$(uname -m)
        log_success "检测到 macOS 系统: $OS_VERSION ($ARCH)"
    elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
        # Windows (MSYS2/Cygwin)
        OS="windows"
        OS_VERSION=$(cmd.exe /c "ver" 2>/dev/null | grep -o "Version [0-9.]*" | cut -d' ' -f2)
        ARCH=$(uname -m)
        log_success "检测到 Windows 系统: $OS_VERSION ($ARCH)"
    else
        log_error "不支持的操作系统: $OSTYPE"
        exit 1
    fi
}

# 检查命令是否存在
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Linux系统依赖安装
install_linux_deps() {
    log_info "安装 Linux 依赖..."
    
    # 检测包管理器并安装依赖
    if command_exists apt-get; then
        # Debian/Ubuntu系列
        log_info "使用 apt 安装依赖..."
        sudo apt-get update
        sudo apt-get install -y \
            build-essential \
            cmake \
            git \
            python3 \
            python3-pip \
            mingw-w64 \
            gcc-aarch64-linux-gnu \
            g++-aarch64-linux-gnu \
            wget \
            curl \
            zip \
            unzip \
            tar
            
    elif command_exists yum; then
        # RHEL/CentOS/Fedora系列
        log_info "使用 yum/dnf 安装依赖..."
        if command_exists dnf; then
            PKG_MANAGER="dnf"
        else
            PKG_MANAGER="yum"
        fi
        
        sudo $PKG_MANAGER install -y \
            gcc \
            gcc-c++ \
            cmake \
            git \
            python3 \
            python3-pip \
            mingw64-gcc \
            mingw64-gcc-c++ \
            wget \
            curl \
            zip \
            unzip \
            tar
            
    elif command_exists pacman; then
        # Arch Linux系列
        log_info "使用 pacman 安装依赖..."
        sudo pacman -S --noconfirm \
            base-devel \
            cmake \
            git \
            python \
            python-pip \
            mingw-w64-gcc \
            wget \
            curl \
            zip \
            unzip \
            tar
            
    else
        log_warning "未识别的 Linux 发行版，请手动安装以下依赖:"
        echo "  - build-essential / gcc-c++"
        echo "  - cmake"
        echo "  - git"
        echo "  - python3 python3-pip"
        echo "  - mingw-w64 (交叉编译 Windows)"
        echo "  - wget curl zip unzip tar"
        read -p "依赖已安装？按 Enter 继续或 Ctrl+C 退出..." -r
    fi
}

# macOS系统依赖安装
install_macos_deps() {
    log_info "安装 macOS 依赖..."
    
    # 检查并安装 Xcode Command Line Tools
    if ! xcode-select -p &>/dev/null; then
        log_info "安装 Xcode Command Line Tools..."
        xcode-select --install
        log_warning "请完成 Xcode 安装后重新运行此脚本"
        exit 1
    fi
    
    # 检查并安装 Homebrew
    if ! command_exists brew; then
        log_info "安装 Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi
    
    # 安装依赖
    log_info "使用 Homebrew 安装依赖..."
    brew update
    brew install \
        cmake \
        git \
        python3 \
        mingw-w64 \
        wget \
        curl \
        zip \
        unzip
}

# Windows系统依赖安装
install_windows_deps() {
    log_info "检查 Windows 依赖..."
    
    # 检查 MSYS2 环境
    if [[ "$OSTYPE" == "msys" ]]; then
        log_info "在 MSYS2 环境中安装依赖..."
        pacman -S --noconfirm \
            mingw-w64-x86_64-gcc \
            mingw-w64-x86_64-cmake \
            mingw-w64-x86_64-make \
            git \
            python3 \
            python3-pip \
            wget \
            curl \
            zip \
            unzip \
            tar
    else
        log_warning "Windows 环境建议使用 MSYS2 或 WSL"
        log_info "请确保已安装以下工具:"
        echo "  - MinGW-w64 (GCC 编译器)"
        echo "  - CMake"
        echo "  - Git"
        echo "  - Python 3"
        read -p "依赖已安装？按 Enter 继续或 Ctrl+C 退出..." -r
    fi
}

# Python依赖安装
install_python_deps() {
    log_info "安装 Python GUI 依赖..."
    
    if command_exists python3; then
        PYTHON_CMD="python3"
    elif command_exists python; then
        PYTHON_CMD="python"
    else
        log_error "未找到 Python，请先安装 Python 3.8+"
        exit 1
    fi
    
    # 检查Python版本
    PYTHON_VERSION=$($PYTHON_CMD --version 2>&1 | cut -d' ' -f2)
    log_info "检测到 Python 版本: $PYTHON_VERSION"
    
    # 安装GUI依赖
    if [ -f "$WORK_DIR/source_python_gui/requirements.txt" ]; then
        log_info "安装 GUI 依赖包..."
        $PYTHON_CMD -m pip install --user -r "$WORK_DIR/source_python_gui/requirements.txt"
    else
        log_warning "未找到 requirements.txt，手动安装核心依赖..."
        $PYTHON_CMD -m pip install --user PySide6 numpy
    fi
}

# 主依赖安装函数
install_dependencies() {
    log_header "🔧 安装系统依赖"
    
    case "$OS" in
        "ubuntu"|"debian"|"linuxmint"|"pop"|"elementary")
            install_linux_deps
            ;;
        "fedora"|"rhel"|"centos"|"rocky"|"almalinux")
            install_linux_deps
            ;;
        "arch"|"manjaro"|"endeavouros")
            install_linux_deps
            ;;
        "macos")
            install_macos_deps
            ;;
        "windows")
            install_windows_deps
            ;;
        *)
            log_warning "未知系统，尝试通用 Linux 安装..."
            install_linux_deps
            ;;
    esac
    
    # 安装Python依赖
    install_python_deps
    
    log_success "依赖安装完成"
}

# 构建嵌入式ARM端
build_embedded() {
    log_header "🎯 构建嵌入式 ARM 端"
    
    cd "$WORK_DIR/source_linux_armv7l"
    
    log_info "配置 ARM 构建..."
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
    
    log_info "编译 ARM 二进制文件..."
    cmake --build build --config Release -j$(nproc 2>/dev/null || echo 4)
    
    log_info "创建嵌入式发布包..."
    ./create_release.sh
    
    log_success "ARM 端构建完成"
    cd "$WORK_DIR"
}

# 构建PC跨平台端
build_pc_platforms() {
    log_header "🖥️ 构建 PC 跨平台端"
    
    cd "$WORK_DIR/source_all_platform"
    
    log_info "构建所有 PC 平台..."
    if [ -f "./build_all_platforms.sh" ]; then
        chmod +x ./build_all_platforms.sh
        ./build_all_platforms.sh all
    else
        log_warning "未找到 build_all_platforms.sh，使用基本构建..."
        mkdir -p build && cd build
        cmake .. -DCMAKE_BUILD_TYPE=Release
        make -j$(nproc 2>/dev/null || echo 4)
        cd ..
    fi
    
    # 构建动态库给Python GUI使用
    log_info "构建 Python GUI 动态库..."
    mkdir -p build_shared && cd build_shared
    cmake .. -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc 2>/dev/null || echo 4)
    
    # 复制动态库到GUI目录
    if [ -f "libv4l2_usb_pc.so" ]; then
        cp libv4l2_usb_pc.so "$WORK_DIR/source_python_gui/"
        log_success "动态库已复制到 GUI 目录"
    elif [ -f "libv4l2_usb_pc.dll" ]; then
        cp libv4l2_usb_pc.dll "$WORK_DIR/source_python_gui/"
        log_success "动态库已复制到 GUI 目录"
    elif [ -f "libv4l2_usb_pc.dylib" ]; then
        cp libv4l2_usb_pc.dylib "$WORK_DIR/source_python_gui/"
        log_success "动态库已复制到 GUI 目录"
    else
        log_warning "未找到动态库文件"
    fi
    
    cd "$WORK_DIR/source_all_platform"
    
    log_info "创建 PC 端发布包..."
    if [ -f "./create_release.sh" ]; then
        ./create_release.sh
    fi
    
    log_success "PC 端构建完成"
    cd "$WORK_DIR"
}

# 测试Python GUI
test_python_gui() {
    log_header "🎨 测试 Python GUI"
    
    cd "$WORK_DIR/source_python_gui"
    
    # 检查依赖是否正确安装
    if command_exists python3; then
        PYTHON_CMD="python3"
    elif command_exists python; then
        PYTHON_CMD="python"
    else
        log_error "Python 未安装"
        return 1
    fi
    
    log_info "测试 Python 导入..."
    if $PYTHON_CMD -c "import PySide6; import numpy; print('✅ Python 依赖正常')" 2>/dev/null; then
        log_success "Python 依赖测试通过"
    else
        log_warning "Python 依赖测试失败，GUI 可能无法运行"
    fi
    
    # 检查动态库
    if [ -f "libv4l2_usb_pc.so" ] || [ -f "libv4l2_usb_pc.dll" ] || [ -f "libv4l2_usb_pc.dylib" ]; then
        log_success "动态库文件存在"
    else
        log_warning "动态库文件缺失，GUI 将使用模拟模式"
    fi
    
    cd "$WORK_DIR"
}

# 创建统一发布包
create_unified_package() {
    log_header "📦 创建统一发布包"
    
    PACKAGE_DIR="$WORK_DIR/$FINAL_PACKAGE_NAME"
    
    log_info "创建发布包目录: $FINAL_PACKAGE_NAME"
    rm -rf "$PACKAGE_DIR"
    mkdir -p "$PACKAGE_DIR"
    
    # 1. 复制嵌入式端
    log_info "打包嵌入式 ARM 端..."
    if [ -d "$WORK_DIR/source_linux_armv7l/build" ]; then
        mkdir -p "$PACKAGE_DIR/embedded_arm"
        cp -r "$WORK_DIR/source_linux_armv7l/build" "$PACKAGE_DIR/embedded_arm/"
        
        # 复制启动脚本
        if [ -f "$WORK_DIR/source_linux_armv7l/create_release.sh" ]; then
            cp "$WORK_DIR/source_linux_armv7l/create_release.sh" "$PACKAGE_DIR/embedded_arm/"
        fi
        
        # 复制源码
        cp "$WORK_DIR/source_linux_armv7l"/*.c "$PACKAGE_DIR/embedded_arm/" 2>/dev/null || true
        cp "$WORK_DIR/source_linux_armv7l/CMakeLists.txt" "$PACKAGE_DIR/embedded_arm/" 2>/dev/null || true
        
        log_success "嵌入式端打包完成"
    else
        log_warning "嵌入式端构建目录不存在"
    fi
    
    # 2. 复制PC端
    log_info "打包 PC 跨平台端..."
    if [ -d "$WORK_DIR/source_all_platform" ]; then
        mkdir -p "$PACKAGE_DIR/pc_platforms"
        
        # 复制构建结果
        [ -d "$WORK_DIR/source_all_platform/build" ] && cp -r "$WORK_DIR/source_all_platform/build" "$PACKAGE_DIR/pc_platforms/" || true
        [ -d "$WORK_DIR/source_all_platform/build_shared" ] && cp -r "$WORK_DIR/source_all_platform/build_shared" "$PACKAGE_DIR/pc_platforms/" || true
        
        # 复制源码和脚本
        cp "$WORK_DIR/source_all_platform"/*.c "$PACKAGE_DIR/pc_platforms/" 2>/dev/null || true
        cp "$WORK_DIR/source_all_platform"/*.h "$PACKAGE_DIR/pc_platforms/" 2>/dev/null || true
        cp "$WORK_DIR/source_all_platform/CMakeLists.txt" "$PACKAGE_DIR/pc_platforms/" 2>/dev/null || true
        cp "$WORK_DIR/source_all_platform"/*.sh "$PACKAGE_DIR/pc_platforms/" 2>/dev/null || true
        cp "$WORK_DIR/source_all_platform"/*.py "$PACKAGE_DIR/pc_platforms/" 2>/dev/null || true
        
        log_success "PC 端打包完成"
    else
        log_warning "PC 端目录不存在"
    fi
    
    # 3. 复制Python GUI
    log_info "打包 Python GUI..."
    if [ -d "$WORK_DIR/source_python_gui" ]; then
        mkdir -p "$PACKAGE_DIR/python_gui"
        cp -r "$WORK_DIR/source_python_gui"/* "$PACKAGE_DIR/python_gui/"
        
        log_success "Python GUI 打包完成"
    else
        log_warning "Python GUI 目录不存在"
    fi
    
    # 4. 创建启动脚本
    log_info "创建启动脚本..."
    
    # Linux/macOS 启动脚本
    cat > "$PACKAGE_DIR/run_gui.sh" << 'EOF'
#!/bin/bash
cd python_gui
if command -v python3 >/dev/null 2>&1; then
    python3 main.py
elif command -v python >/dev/null 2>&1; then
    python main.py
else
    echo "Error: Python not found. Please install Python 3.8+"
    exit 1
fi
EOF
    chmod +x "$PACKAGE_DIR/run_gui.sh"
    
    # Windows 启动脚本
    cat > "$PACKAGE_DIR/run_gui.bat" << 'EOF'
@echo off
cd python_gui
python main.py
if %ERRORLEVEL% neq 0 (
    echo Error: Failed to run Python GUI
    pause
)
EOF
    
    # 嵌入式端部署脚本
    cat > "$PACKAGE_DIR/deploy_embedded.sh" << 'EOF'
#!/bin/bash
DEVICE_IP="172.32.0.93"
if [ "$1" != "" ]; then
    DEVICE_IP="$1"
fi

echo "部署到设备: $DEVICE_IP"
echo "上传嵌入式程序..."

scp embedded_arm/build/* root@$DEVICE_IP:/root/
scp embedded_arm/*.sh root@$DEVICE_IP:/root/ 2>/dev/null || true

echo "部署完成！"
echo "登录设备运行: ssh root@$DEVICE_IP"
echo "启动服务器: ./v4l2_usb"
EOF
    chmod +x "$PACKAGE_DIR/deploy_embedded.sh"
    
    # 5. 创建文档
    log_info "创建说明文档..."
    cat > "$PACKAGE_DIR/README.md" << EOF
# Luckfox Pico V4L2 完整发布包 v2.0

构建日期: $(date)
构建系统: $OS $OS_VERSION ($ARCH)

## 📁 目录结构

- \`embedded_arm/\` - 嵌入式 ARM 端程序
- \`pc_platforms/\` - PC 跨平台端程序
- \`python_gui/\` - Python GUI 图形界面
- \`run_gui.sh/bat\` - GUI 启动脚本
- \`deploy_embedded.sh\` - 嵌入式端部署脚本

## 🚀 快速启动

### 1. 部署嵌入式端
\`\`\`bash
# 自动部署到默认设备 (172.32.0.93)
./deploy_embedded.sh

# 或部署到自定义IP
./deploy_embedded.sh 192.168.1.100
\`\`\`

### 2. 启动 Python GUI
\`\`\`bash
# Linux/macOS
./run_gui.sh

# Windows
run_gui.bat
\`\`\`

### 3. 手动运行
\`\`\`bash
# 命令行版本
cd pc_platforms/build
./v4l2_usb_pc -s 172.32.0.93 -c

# Python GUI
cd python_gui
python main.py
\`\`\`

## 🔧 系统要求

- **嵌入式端**: Luckfox Pico + SC3336 摄像头
- **PC 端**: 
  - Linux: Ubuntu 18.04+ / 其他主流发行版
  - Windows: Windows 10+ (建议使用 MSYS2)
  - macOS: macOS 10.15+
- **Python GUI**: Python 3.8+, PySide6, NumPy

## 📊 功能特性

- 🎥 2304×1296 RAW10 图像采集
- 📡 TCP 网络实时推流
- 🖼️ 16-bit RAW 图像显示 (类似 ImageJ)
- 🎨 自动对比度 + 伪彩色映射
- 💾 仅内存 / 文件保存模式
- 📈 实时性能监控

---
自动构建于: $(date) by build.sh v$SCRIPT_VERSION
EOF
    
    # 6. 创建压缩包
    log_info "创建压缩包..."
    cd "$WORK_DIR"
    
    if command_exists tar; then
        tar -czf "${FINAL_PACKAGE_NAME}.tar.gz" "$FINAL_PACKAGE_NAME"
        log_success "已创建: ${FINAL_PACKAGE_NAME}.tar.gz ($(du -h "${FINAL_PACKAGE_NAME}.tar.gz" | cut -f1))"
    fi
    
    if command_exists zip; then
        zip -r "${FINAL_PACKAGE_NAME}.zip" "$FINAL_PACKAGE_NAME" >/dev/null
        log_success "已创建: ${FINAL_PACKAGE_NAME}.zip ($(du -h "${FINAL_PACKAGE_NAME}.zip" | cut -f1))"
    fi
    
    log_success "统一发布包创建完成！"
    echo
    echo "📦 发布包内容："
    echo "   目录: $FINAL_PACKAGE_NAME/"
    [ -f "${FINAL_PACKAGE_NAME}.tar.gz" ] && echo "   压缩包: ${FINAL_PACKAGE_NAME}.tar.gz"
    [ -f "${FINAL_PACKAGE_NAME}.zip" ] && echo "   压缩包: ${FINAL_PACKAGE_NAME}.zip"
}

# 主函数
main() {
    log_header "🚀 Luckfox Pico V4L2 全自动构建脚本 v$SCRIPT_VERSION"
    
    echo "此脚本将执行以下操作："
    echo "1. 🔍 检测系统环境"
    echo "2. 🔧 自动安装依赖"
    echo "3. 🎯 构建嵌入式 ARM 端"
    echo "4. 🖥️ 构建 PC 跨平台端"
    echo "5. 🎨 构建 Python GUI"
    echo "6. 📦 创建统一发布包"
    echo
    
    read -p "是否继续？(y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_info "用户取消操作"
        exit 0
    fi
    
    # 开始构建流程
    detect_system
    install_dependencies
    build_embedded
    build_pc_platforms  
    test_python_gui
    create_unified_package
    
    # 构建完成总结
    log_header "🎉 构建完成！"
    echo
    echo "✅ 嵌入式 ARM 端: $([ -d "source_linux_armv7l/build" ] && echo "成功" || echo "失败")"
    echo "✅ PC 跨平台端: $([ -d "source_all_platform/build" ] && echo "成功" || echo "失败")"  
    echo "✅ Python GUI: $([ -f "source_python_gui/main.py" ] && echo "成功" || echo "失败")"
    echo "✅ 统一发布包: $([ -d "$FINAL_PACKAGE_NAME" ] && echo "成功" || echo "失败")"
    echo
    
    if [ -f "${FINAL_PACKAGE_NAME}.tar.gz" ] || [ -f "${FINAL_PACKAGE_NAME}.zip" ]; then
        log_success "🎯 发布包已就绪，可以分发使用！"
        echo
        echo "📋 下一步操作："
        echo "   1. 解压发布包到目标系统"
        echo "   2. 运行 ./deploy_embedded.sh 部署嵌入式端"
        echo "   3. 运行 ./run_gui.sh 启动 Python GUI"
        echo "   4. 在 GUI 中连接到 172.32.0.93:8888"
    else
        log_warning "发布包创建可能有问题，请检查构建日志"
    fi
    
    log_success "构建脚本执行完成！"
}

# 执行主函数
main "$@"