cmake_minimum_required(VERSION 3.16)
project(v4l2_bench C)

# 设置交叉编译
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 指定工具链路径
set(TOOLCHAIN_PATH "/home/liewzheng/Workspace/luckfox-pico/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin")
set(TOOLCHAIN_PREFIX "arm-rockchip830-linux-uclibcgnueabihf")

# 设置交叉编译器
set(CMAKE_C_COMPILER ${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}-g++)

# 设置其他工具
set(CMAKE_AR ${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}-ar CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB ${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}-ranlib CACHE FILEPATH "Ranlib")
set(CMAKE_OBJCOPY ${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}-objcopy CACHE FILEPATH "Object Copy")
set(CMAKE_STRIP ${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}-strip CACHE FILEPATH "Strip")
set(CMAKE_SIZE ${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}-size CACHE FILEPATH "Size")

# 设置系统根目录
set(SYSROOT ${TOOLCHAIN_PATH}/../arm-rockchip830-linux-uclibcgnueabihf/sysroot)
set(CMAKE_SYSROOT ${SYSROOT})
set(CMAKE_FIND_ROOT_PATH ${SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 编译器标志
set(CMAKE_C_FLAGS "-O3 -march=armv8-a -mtune=cortex-a55 -mfpu=neon-fp-armv8 -mfloat-abi=hard -ffast-math")
set(CMAKE_EXE_LINKER_FLAGS "-Wl,--gc-sections -lrt")

# 添加可执行文件
add_executable(v4l2_bench source_linux_armv7l/v4l2_bench.c)
add_executable(v4l2_bench_mp source_linux_armv7l/v4l2_bench_mp.c)
add_executable(test_multiplanar source_linux_armv7l/test_multiplanar.c)
add_executable(v4l2_usb source_linux_armv7l/v4l2_usb.c)

# 链接数学库（如果需要浮点运算）
target_link_libraries(v4l2_bench m rt)
target_link_libraries(v4l2_bench_mp m rt)
target_link_libraries(test_multiplanar m rt)
target_link_libraries(v4l2_usb m rt pthread)

# 安装规则（可选）
install(TARGETS v4l2_bench v4l2_bench_mp test_multiplanar v4l2_usb
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ)

# 添加自定义目标用于部署（可选）
add_custom_target(deploy
    COMMAND scp v4l2_bench v4l2_bench_mp test_multiplanar v4l2_usb ${TARGET_USER}@${TARGET_IP}:${TARGET_PATH}
    COMMENT "Deploying executables to target device"
    DEPENDS v4l2_bench v4l2_bench_mp test_multiplanar v4l2_usb)