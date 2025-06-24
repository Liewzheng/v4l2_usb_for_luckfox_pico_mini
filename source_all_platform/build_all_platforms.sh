#!/bin/bash

# build_all_platforms.sh - 跨平台构建脚本
# 支持Linux x86_64、Windows x86_64、macOS ARM64的交叉编译

set -e  # 遇到错误时停止

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
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

# 检查工具是否存在
check_tool() {
    if command -v "$1" &> /dev/null; then
        log_success "$1 found"
        return 0
    else
        log_warning "$1 not found"
        return 1
    fi
}

# 构建函数
build_platform() {
    local platform=$1
    local toolchain_file=$2
    local additional_args=$3
    
    log_info "Building for ${platform}..."
    
    local build_dir="build_${platform}"
    
    # 清理并创建构建目录
    rm -rf "${build_dir}"
    mkdir -p "${build_dir}"
    
    cd "${build_dir}"
    
    # 配置CMake
    local cmake_args="-DCMAKE_BUILD_TYPE=Release"
    if [ -n "${toolchain_file}" ]; then
        cmake_args="${cmake_args} -DCMAKE_TOOLCHAIN_FILE=${toolchain_file}"
    fi
    if [ -n "${additional_args}" ]; then
        cmake_args="${cmake_args} ${additional_args}"
    fi
    
    log_info "CMake configuration: cmake ${cmake_args} .."
    cmake ${cmake_args} ..
    
    # 构建
    # macOS 使用 sysctl -n hw.ncpu 获取 CPU 核心数，Linux 使用 nproc
    if [[ "$OSTYPE" == "darwin"* ]]; then
        CPU_CORES=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
    else
        CPU_CORES=$(nproc 2>/dev/null || echo 4)
    fi
    cmake --build . --config Release --parallel ${CPU_CORES}
    
    # 显示构建信息
    cmake --build . --target build_info
    
    cd ..
    
    log_success "${platform} build completed"
}

# 主函数
main() {
    log_info "V4L2 USB PC Client - Multi-Platform Build Script"
    log_info "================================================="
    
    # 检查基本工具
    if ! check_tool "cmake"; then
        log_error "CMake is required but not installed"
        exit 1
    fi
    
    # 获取当前目录
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    cd "${SCRIPT_DIR}"
    
    # 检查源文件
    if [ ! -f "v4l2_usb_pc.h" ] || [ ! -f "v4l2_usb_pc_core.c" ] || [ ! -f "v4l2_usb_pc_main.c" ]; then
        log_error "Source files not found in current directory"
        exit 1
    fi
    
    # 创建工具链文件目录
    mkdir -p toolchains
    
    # 创建Windows x86_64工具链文件
    cat > toolchains/windows_x86_64.cmake << 'EOF'
# Windows x86_64 工具链文件
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# 设置编译器
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# 设置查找路径
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# 设置可执行文件后缀
set(CMAKE_EXECUTABLE_SUFFIX ".exe")
EOF

    # 创建macOS ARM64工具链文件
    cat > toolchains/macos_arm64.cmake << 'EOF'
# macOS ARM64 工具链文件
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR arm64)

# 注意：这需要macOS SDK和适当的交叉编译工具链
# 在Linux上交叉编译macOS需要额外的工具和SDK
set(CMAKE_OSX_ARCHITECTURES arm64)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")
EOF
    
    log_info "Available build targets:"
    echo "  1. native   - Native platform (current system)"
    echo "  2. windows  - Windows x86_64 (requires MinGW-w64)"
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "  3. macos    - macOS native build (current system)"
    else
        echo "  3. macos    - macOS ARM64 (cross-compilation, requires osxcross)"
    fi
    echo "  4. all      - All supported platforms"
    
    # 解析命令行参数
    TARGET="${1:-native}"
    
    case "${TARGET}" in
        "native")
            log_info "Building for native platform..."
            build_platform "native" "" ""
            ;;
            
        "windows")
            if check_tool "x86_64-w64-mingw32-gcc"; then
                build_platform "windows_x86_64" "toolchains/windows_x86_64.cmake" ""
            else
                log_error "MinGW-w64 not found. Install with: sudo apt install gcc-mingw-w64-x86-64"
                exit 1
            fi
            ;;
            
        "macos")
            # 检测当前是否在 macOS 上运行
            if [[ "$OSTYPE" == "darwin"* ]]; then
                log_info "Building natively on macOS..."
                build_platform "macos_native" "" ""
            else
                log_warning "macOS cross-compilation is experimental and requires macOS SDK"
                if [ -d "/opt/osxcross" ]; then
                    export PATH="/opt/osxcross/bin:$PATH"
                    build_platform "macos_arm64" "toolchains/macos_arm64.cmake" ""
                else
                    log_error "macOS cross-compilation toolchain not found"
                    log_info "You need to set up osxcross: https://github.com/tpoechtrager/osxcross"
                    exit 1
                fi
            fi
            ;;
            
        "all")
            log_info "Building all supported platforms..."
            
            # Native build
            build_platform "native" "" ""
            
            # Windows build (if available)
            if check_tool "x86_64-w64-mingw32-gcc"; then
                build_platform "windows_x86_64" "toolchains/windows_x86_64.cmake" ""
            else
                log_warning "Skipping Windows build - MinGW-w64 not found"
            fi
            
            # macOS build (if available)
            if [[ "$OSTYPE" == "darwin"* ]]; then
                log_info "Building natively on macOS..."
                build_platform "macos_native" "" ""
            elif [ -d "/opt/osxcross" ]; then
                export PATH="/opt/osxcross/bin:$PATH"
                build_platform "macos_arm64" "toolchains/macos_arm64.cmake" ""
            else
                log_warning "Skipping macOS build - osxcross not found"
            fi
            ;;
            
        *)
            log_error "Unknown target: ${TARGET}"
            echo "Usage: $0 [native|windows|macos|all]"
            exit 1
            ;;
    esac
    
    # 显示构建结果
    log_info "Build Results:"
    log_info "============="
    
    for build_dir in build_*; do
        if [ -d "${build_dir}" ]; then
            log_info "Build directory: ${build_dir}"
            dist_dir="${build_dir}/dist"
            if [ -d "${dist_dir}" ]; then
                # macOS 和 Linux 的 find 命令语法不同
                if [[ "$OSTYPE" == "darwin"* ]]; then
                    find "${dist_dir}" -type f \( -perm +111 -o -name "*.exe" -o -name "*.so" -o -name "*.dylib" -o -name "*.a" \) | while read -r file; do
                        size=$(du -h "${file}" | cut -f1)
                        log_success "  $(basename "${file}") (${size}) - ${file}"
                    done
                else
                    find "${dist_dir}" -type f -executable -o -name "*.exe" -o -name "*.so" -o -name "*.dylib" -o -name "*.a" | while read -r file; do
                        size=$(du -h "${file}" | cut -f1)
                        log_success "  $(basename "${file}") (${size}) - ${file}"
                    done
                fi
            fi
        fi
    done
    
    log_success "Multi-platform build completed!"
    if [[ "$OSTYPE" != "darwin"* ]]; then
        log_info "Install MinGW-w64 for Windows cross-compilation: sudo apt install gcc-mingw-w64-x86-64"
        log_info "Install osxcross for macOS cross-compilation: https://github.com/tpoechtrager/osxcross"
    else
        log_info "For cross-compilation on macOS:"
        log_info "  Windows: Install MinGW-w64 via Homebrew: brew install mingw-w64"
        log_info "  Linux: Use Docker or cross-compilation toolchain"
    fi
}

# 运行主函数
main "$@"
