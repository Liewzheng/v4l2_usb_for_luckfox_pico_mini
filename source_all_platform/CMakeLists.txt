cmake_minimum_required(VERSION 3.16)
project(v4l2_usb_pc VERSION 2.0.0 LANGUAGES C)

# 设置C标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 设置构建类型（如果未指定）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 平台检测
if(WIN32)
    set(TARGET_PLATFORM "windows")
    set(TARGET_ARCH "x86_64")
elseif(APPLE)
    set(TARGET_PLATFORM "macos")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(TARGET_ARCH "arm64")
    else()
        set(TARGET_ARCH "x86_64")
    endif()
else()
    set(TARGET_PLATFORM "linux")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(TARGET_ARCH "arm64")
    else()
        set(TARGET_ARCH "x86_64")
    endif()
endif()

message(STATUS "Target platform: ${TARGET_PLATFORM}")
message(STATUS "Target architecture: ${TARGET_ARCH}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# 输出目录设置
set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/dist/${TARGET_PLATFORM}_${TARGET_ARCH}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR}/lib)

# 源文件
set(CORE_SOURCES
    v4l2_usb_pc_core.c
)

set(MAIN_SOURCES
    v4l2_usb_pc_main.c
)

set(HEADERS
    v4l2_usb_pc.h
)

# 创建静态库
add_library(v4l2_usb_pc_static STATIC ${CORE_SOURCES})
target_include_directories(v4l2_usb_pc_static PUBLIC .)

# 创建动态库
add_library(v4l2_usb_pc_shared SHARED ${CORE_SOURCES})
target_include_directories(v4l2_usb_pc_shared PUBLIC .)

# 创建可执行文件
add_executable(v4l2_usb_pc ${MAIN_SOURCES})
target_include_directories(v4l2_usb_pc PRIVATE .)
target_link_libraries(v4l2_usb_pc v4l2_usb_pc_static)

# 平台特定配置
if(WIN32)
    # Windows配置
    target_compile_definitions(v4l2_usb_pc_static PRIVATE _WIN32_WINNT=0x0600)
    target_compile_definitions(v4l2_usb_pc_shared PRIVATE _WIN32_WINNT=0x0600)
    target_compile_definitions(v4l2_usb_pc PRIVATE _WIN32_WINNT=0x0600)
    
    # Windows链接库
    target_link_libraries(v4l2_usb_pc_static ws2_32)
    target_link_libraries(v4l2_usb_pc_shared ws2_32)
    target_link_libraries(v4l2_usb_pc ws2_32)
    
    # 设置输出文件名
    set_target_properties(v4l2_usb_pc PROPERTIES OUTPUT_NAME "v4l2_usb_pc")
    set_target_properties(v4l2_usb_pc_static PROPERTIES OUTPUT_NAME "v4l2_usb_pc_static")
    set_target_properties(v4l2_usb_pc_shared PROPERTIES OUTPUT_NAME "v4l2_usb_pc")
    
elseif(APPLE)
    # macOS配置
    target_compile_definitions(v4l2_usb_pc_static PRIVATE _DARWIN_C_SOURCE)
    target_compile_definitions(v4l2_usb_pc_shared PRIVATE _DARWIN_C_SOURCE)
    target_compile_definitions(v4l2_usb_pc PRIVATE _DARWIN_C_SOURCE)
    
    # macOS链接库
    find_library(CORE_FOUNDATION CoreFoundation)
    target_link_libraries(v4l2_usb_pc_static pthread ${CORE_FOUNDATION})
    target_link_libraries(v4l2_usb_pc_shared pthread ${CORE_FOUNDATION})
    target_link_libraries(v4l2_usb_pc pthread)
    
    # 设置输出文件名
    set_target_properties(v4l2_usb_pc PROPERTIES OUTPUT_NAME "v4l2_usb_pc")
    set_target_properties(v4l2_usb_pc_static PROPERTIES OUTPUT_NAME "v4l2_usb_pc_static")
    set_target_properties(v4l2_usb_pc_shared PROPERTIES OUTPUT_NAME "v4l2_usb_pc")
    
else()
    # Linux配置
    target_compile_definitions(v4l2_usb_pc_static PRIVATE _GNU_SOURCE)
    target_compile_definitions(v4l2_usb_pc_shared PRIVATE _GNU_SOURCE)
    target_compile_definitions(v4l2_usb_pc PRIVATE _GNU_SOURCE)
    
    # Linux链接库
    target_link_libraries(v4l2_usb_pc_static pthread m)
    target_link_libraries(v4l2_usb_pc_shared pthread m)
    target_link_libraries(v4l2_usb_pc pthread m)
    
    # 设置输出文件名
    set_target_properties(v4l2_usb_pc PROPERTIES OUTPUT_NAME "v4l2_usb_pc")
    set_target_properties(v4l2_usb_pc_static PROPERTIES OUTPUT_NAME "v4l2_usb_pc_static")
    set_target_properties(v4l2_usb_pc_shared PROPERTIES OUTPUT_NAME "v4l2_usb_pc")
endif()

# 编译器特定优化
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    # 根据目标平台选择合适的架构优化
    set(ARCH_FLAGS "")
    if(WIN32)
        # Windows x86_64 交叉编译使用通用的 x86_64 优化
        set(ARCH_FLAGS "-march=x86-64;-mtune=generic")
    elseif(APPLE)
        # macOS 本地编译可以使用 native
        if(CMAKE_CROSSCOMPILING)
            set(ARCH_FLAGS "-march=armv8-a")
        else()
            set(ARCH_FLAGS "-march=native")
        endif()
    else()
        # Linux 本地编译可以使用 native，交叉编译使用通用设置
        if(CMAKE_CROSSCOMPILING)
            # 检查目标架构
            if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
                set(ARCH_FLAGS "-march=x86-64;-mtune=generic")
            elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
                set(ARCH_FLAGS "-march=armv8-a")
            else()
                set(ARCH_FLAGS "-march=native")
            endif()
        else()
            set(ARCH_FLAGS "-march=native")
        endif()
    endif()
    
    # GCC/Clang优化选项
    target_compile_options(v4l2_usb_pc_static PRIVATE 
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O3 ${ARCH_FLAGS} -DNDEBUG>
        $<$<CONFIG:Debug>:-O0 -g3 -DDEBUG>
    )
    target_compile_options(v4l2_usb_pc_shared PRIVATE 
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O3 ${ARCH_FLAGS} -DNDEBUG>
        $<$<CONFIG:Debug>:-O0 -g3 -DDEBUG>
    )
    target_compile_options(v4l2_usb_pc PRIVATE 
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O3 ${ARCH_FLAGS} -DNDEBUG>
        $<$<CONFIG:Debug>:-O0 -g3 -DDEBUG>
    )
    
    # SIMD支持检测
    include(CheckCSourceCompiles)
    
    # 对于交叉编译到 Windows x86_64，我们可以安全地假设 AVX2 支持
    if(WIN32 AND CMAKE_CROSSCOMPILING)
        # Windows x86_64 交叉编译默认启用 AVX2
        target_compile_options(v4l2_usb_pc_static PRIVATE -mavx2)
        target_compile_options(v4l2_usb_pc_shared PRIVATE -mavx2)
        target_compile_options(v4l2_usb_pc PRIVATE -mavx2)
        target_compile_definitions(v4l2_usb_pc_static PRIVATE __AVX2__)
        target_compile_definitions(v4l2_usb_pc_shared PRIVATE __AVX2__)
        target_compile_definitions(v4l2_usb_pc PRIVATE __AVX2__)
        message(STATUS "AVX2 support enabled (cross-compilation default)")
    else()
        # 本地编译时进行 SIMD 检测
        # 检测AVX2支持
        set(CMAKE_REQUIRED_FLAGS "-mavx2")
        check_c_source_compiles("
            #include <immintrin.h>
            int main() {
                __m256i a = _mm256_setzero_si256();
                return 0;
            }
        " HAVE_AVX2)
        
        if(HAVE_AVX2)
            target_compile_options(v4l2_usb_pc_static PRIVATE -mavx2)
            target_compile_options(v4l2_usb_pc_shared PRIVATE -mavx2)
            target_compile_options(v4l2_usb_pc PRIVATE -mavx2)
            target_compile_definitions(v4l2_usb_pc_static PRIVATE __AVX2__)
            target_compile_definitions(v4l2_usb_pc_shared PRIVATE __AVX2__)
            target_compile_definitions(v4l2_usb_pc PRIVATE __AVX2__)
            message(STATUS "AVX2 support enabled")
        else()
            # 检测SSE2支持
            set(CMAKE_REQUIRED_FLAGS "-msse2")
            check_c_source_compiles("
                #include <emmintrin.h>
                int main() {
                    __m128i a = _mm_setzero_si128();
                    return 0;
                }
            " HAVE_SSE2)
            
            if(HAVE_SSE2)
                target_compile_options(v4l2_usb_pc_static PRIVATE -msse2)
                target_compile_options(v4l2_usb_pc_shared PRIVATE -msse2)
                target_compile_options(v4l2_usb_pc PRIVATE -msse2)
                target_compile_definitions(v4l2_usb_pc_static PRIVATE __SSE2__)
                target_compile_definitions(v4l2_usb_pc_shared PRIVATE __SSE2__)
                target_compile_definitions(v4l2_usb_pc PRIVATE __SSE2__)
                message(STATUS "SSE2 support enabled")
            else()
                message(STATUS "No SIMD support detected")
            endif()
        endif()
    endif()
    
    set(CMAKE_REQUIRED_FLAGS "")
    
elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    # MSVC优化选项
    target_compile_options(v4l2_usb_pc_static PRIVATE 
        /W4
        $<$<CONFIG:Release>:/O2 /arch:AVX2 /DNDEBUG>
        $<$<CONFIG:Debug>:/Od /Zi /DDEBUG>
    )
    target_compile_options(v4l2_usb_pc_shared PRIVATE 
        /W4
        $<$<CONFIG:Release>:/O2 /arch:AVX2 /DNDEBUG>
        $<$<CONFIG:Debug>:/Od /Zi /DDEBUG>
    )
    target_compile_options(v4l2_usb_pc PRIVATE 
        /W4
        $<$<CONFIG:Release>:/O2 /arch:AVX2 /DNDEBUG>
        $<$<CONFIG:Debug>:/Od /Zi /DDEBUG>
    )
    
    # MSVC AVX2支持
    target_compile_definitions(v4l2_usb_pc_static PRIVATE __AVX2__)
    target_compile_definitions(v4l2_usb_pc_shared PRIVATE __AVX2__)
    target_compile_definitions(v4l2_usb_pc PRIVATE __AVX2__)
endif()

# 安装规则
install(TARGETS v4l2_usb_pc v4l2_usb_pc_static v4l2_usb_pc_shared
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ${HEADERS} DESTINATION include)

# 创建发布包
if(WIN32)
    set(CPACK_GENERATOR "ZIP")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

set(CPACK_PACKAGE_NAME "v4l2_usb_pc")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "V4L2 USB RAW Image Receiver")
set(CPACK_PACKAGE_VENDOR "Development Team")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${TARGET_PLATFORM}-${TARGET_ARCH}")

include(CPack)

# 自定义目标：构建所有平台
add_custom_target(build_info
    COMMAND ${CMAKE_COMMAND} -E echo "Build Information:"
    COMMAND ${CMAKE_COMMAND} -E echo "  Platform: ${TARGET_PLATFORM}"
    COMMAND ${CMAKE_COMMAND} -E echo "  Architecture: ${TARGET_ARCH}"
    COMMAND ${CMAKE_COMMAND} -E echo "  Build Type: ${CMAKE_BUILD_TYPE}"
    COMMAND ${CMAKE_COMMAND} -E echo "  Output Directory: ${OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E echo "  Executable: ${OUTPUT_DIR}/bin/v4l2_usb_pc${CMAKE_EXECUTABLE_SUFFIX}"
    COMMAND ${CMAKE_COMMAND} -E echo "  Static Library: ${OUTPUT_DIR}/lib/libv4l2_usb_pc_static${CMAKE_STATIC_LIBRARY_SUFFIX}"
    COMMAND ${CMAKE_COMMAND} -E echo "  Shared Library: ${OUTPUT_DIR}/lib/libv4l2_usb_pc${CMAKE_SHARED_LIBRARY_SUFFIX}"
)

# 清理自定义目标
add_custom_target(clean_all
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/dist
    COMMENT "Cleaning all build artifacts"
)
