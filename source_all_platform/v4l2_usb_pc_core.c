/**
 * @file v4l2_usb_pc_core.c
 * @brief V4L2图像流跨平台PC客户端核心实现
 *
 * 包含网络通信、图像处理、文件管理等核心功能。
 *
 * @author Development Team
 * @date 2025-06-24
 * @version 2.0
 */

#include "v4l2_usb_pc.h"
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>

// ========================== 全局状态变量 ==========================

/** @brief 程序运行状态标志，0表示停止，1表示运行 */
volatile int running = 1;

/** @brief 主TCP连接socket文件描述符 */
socket_t sock_fd = INVALID_SOCKET_FD;

/** @brief 性能统计信息 */
struct stats stats = {0};

/** @brief 全局内存池 - 解包缓冲区 */
uint16_t* g_unpack_buffer = NULL;
size_t g_buffer_size = 0;

// ========================== 跨平台工具函数 ==========================

/**
 * @brief 获取系统CPU核心数
 */
int get_cpu_cores(void) {
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#elif defined(__APPLE__)
    int num_cores;
    size_t size = sizeof(num_cores);
    if (sysctlbyname("hw.ncpu", &num_cores, &size, NULL, 0) == 0) {
        return num_cores;
    }
    return 1;
#else
    return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

/**
 * @brief 获取高精度时间戳（跨平台实现）
 */
uint64_t get_time_ns(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)(counter.QuadPart * 1000000000ULL / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

/**
 * @brief 跨平台目录创建函数
 */
int create_directory(const char* path)
{
#ifdef _WIN32
    return CreateDirectoryA(path, NULL) ? 0 : -1;
#else
    return mkdir(path, 0755);
#endif
}

/**
 * @brief 跨平台毫秒级延时函数
 */
void sleep_ms(int ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

// ========================== 信号处理函数 ==========================

#ifdef _WIN32
/**
 * @brief Windows控制台信号处理函数
 */
BOOL WINAPI console_handler(DWORD signal)
{
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT)
    {
        printf("\nReceived signal, shutting down...\n");
        running = 0;
        return TRUE;
    }
    return FALSE;
}
#else
/**
 * @brief Linux/Unix信号处理函数
 */
void signal_handler(int sig)
{
    printf("\nReceived signal %d, shutting down...\n", sig);
    running = 0;
}
#endif

// ========================== 网络初始化函数 ==========================

/**
 * @brief 初始化网络子系统（跨平台）
 */
int init_network(void)
{
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        printf("WSAStartup failed: %d\n", result);
        return -1;
    }
    printf("Windows Socket initialized\n");
#endif
    return 0;
}

/**
 * @brief 清理网络子系统（跨平台）
 */
void cleanup_network(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

// ========================== 文件系统管理函数 ==========================

/**
 * @brief 创建图像输出目录（跨平台）
 */
int create_output_dir(const char* dir)
{
#ifdef _WIN32
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(dir, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        if (create_directory(dir) == -1)
        {
            printf("Failed to create output directory: %s\n", dir);
            return -1;
        }
        printf("Created output directory: %s\n", dir);
    }
    else
    {
        FindClose(hFind);
    }
#else
    struct stat st = {0};
    if (stat(dir, &st) == -1)
    {
        if (mkdir(dir, 0755) == -1)
        {
            perror("Failed to create output directory");
            return -1;
        }
        printf("Created output directory: %s\n", dir);
    }
#endif
    return 0;
}

// ========================== 网络通信函数 ==========================

/**
 * @brief 可靠地接收指定字节数的数据
 */
int recv_full(socket_t sock, void* buffer, size_t size)
{
    size_t received = 0;
    uint8_t* ptr = (uint8_t*)buffer;

    while (received < size && running)
    {
        ssize_t result = recv(sock, (char*)(ptr + received), (int)(size - received), 0);

        if (result <= 0)
        {
            if (result == 0)
            {
                printf("Connection closed by server\n");
            }
            else
            {
#ifdef _WIN32
                printf("recv failed: %d\n", WSAGetLastError());
#else
                perror("recv failed");
#endif
            }
            return -1;
        }
        received += result;
    }

    return received == size ? 0 : -1;
}

/**
 * @brief 连接到嵌入式端TCP服务器
 */
socket_t connect_to_server(const char* ip, int port)
{
    socket_t sock;
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET_FD)
    {
#ifdef _WIN32
        printf("socket failed: %d\n", WSAGetLastError());
#else
        perror("socket failed");
#endif
        return INVALID_SOCKET_FD;
    }

    // 设置接收超时
#ifdef _WIN32
    DWORD timeout = RECV_TIMEOUT_SEC * 1000;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) < 0)
    {
        printf("setsockopt failed: %d\n", WSAGetLastError());
        close_socket(sock);
        return INVALID_SOCKET_FD;
    }
#else
    struct timeval timeout;
    timeout.tv_sec = RECV_TIMEOUT_SEC;
    timeout.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        perror("setsockopt failed");
        close_socket(sock);
        return INVALID_SOCKET_FD;
    }
#endif

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0)
    {
        printf("Invalid server IP address: %s\n", ip);
        close_socket(sock);
        return INVALID_SOCKET_FD;
    }

    printf("Connecting to %s:%d...\n", ip, port);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
#ifdef _WIN32
        printf("connect failed: %d\n", WSAGetLastError());
#else
        perror("connect failed");
#endif
        close_socket(sock);
        return INVALID_SOCKET_FD;
    }

    printf("Connected successfully!\n");
    return sock;
}

// ========================== 内存管理函数 ==========================

/**
 * @brief 初始化内存池
 */
void init_memory_pool(void)
{
    // 预分配8MB解包缓冲区
    g_buffer_size = 8 * 1024 * 1024 / sizeof(uint16_t);
    g_unpack_buffer = (uint16_t*)malloc(g_buffer_size * sizeof(uint16_t));
    
    if (g_unpack_buffer) {
        printf("Memory pool initialized: %.1f MB\n", 
               (g_buffer_size * sizeof(uint16_t)) / (1024.0 * 1024.0));
    } else {
        printf("Warning: Failed to allocate memory pool\n");
        g_buffer_size = 0;
    }
}

/**
 * @brief 清理内存池
 */
void cleanup_memory_pool(void)
{
    if (g_unpack_buffer) {
        free(g_unpack_buffer);
        g_unpack_buffer = NULL;
        g_buffer_size = 0;
        printf("Memory pool cleaned up\n");
    }
}

// ========================== SBGGR10解包函数 ==========================

/**
 * @brief SBGGR10格式数据解包（标量版本）
 */
void unpack_sbggr10_scalar(const uint8_t raw_bytes[5], uint16_t pixels[4]) {
    // 重构40位数据
    uint64_t combined = ((uint64_t)raw_bytes[4] << 32) |
                       ((uint64_t)raw_bytes[3] << 24) |
                       ((uint64_t)raw_bytes[2] << 16) |
                       ((uint64_t)raw_bytes[1] << 8)  |
                        (uint64_t)raw_bytes[0];
    
    // 提取4个10位像素值（小端序，从低位开始）
    pixels[0] = (uint16_t)((combined >>  0) & 0x3FF);
    pixels[1] = (uint16_t)((combined >> 10) & 0x3FF);
    pixels[2] = (uint16_t)((combined >> 20) & 0x3FF);
    pixels[3] = (uint16_t)((combined >> 30) & 0x3FF);
}

#ifdef __AVX2__
/**
 * @brief SBGGR10格式数据解包（AVX2优化版本）
 */
void unpack_sbggr10_avx2(const uint8_t *raw_data, uint16_t *output, size_t num_blocks) {
    for (size_t block = 0; block < num_blocks; block++) {
        const uint8_t *src = raw_data + block * 40;
        uint16_t *dst = output + block * 32;
        
        // 暂时使用标量版本的循环展开
        for (int i = 0; i < 8; i++) {
            unpack_sbggr10_scalar(src + i * 5, dst + i * 4);
        }
    }
}
#endif

/**
 * @brief 图像解包工作线程函数
 */
#ifdef _WIN32
unsigned int __stdcall unpack_worker_thread(void *arg) {
#else
void* unpack_worker_thread(void *arg) {
#endif
    struct unpack_task *task = (struct unpack_task*)arg;
    
    size_t raw_pos = task->start_offset;
    size_t pixel_pos = task->start_offset / 5 * 4;
    
#ifdef __AVX2__
    // AVX2优化：批量处理8个5字节块
    size_t avx2_blocks = (task->end_offset - raw_pos) / 40;
    if (avx2_blocks > 0) {
        unpack_sbggr10_avx2(task->raw_data + raw_pos, 
                           task->output_data + pixel_pos, 
                           avx2_blocks);
        raw_pos += avx2_blocks * 40;
        pixel_pos += avx2_blocks * 32;
    }
#endif
    
    // 处理剩余的5字节块（标量版本）
    while (raw_pos + 5 <= task->end_offset) {
        unpack_sbggr10_scalar(task->raw_data + raw_pos, 
                             task->output_data + pixel_pos);
        raw_pos += 5;
        pixel_pos += 4;
    }
    
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/**
 * @brief 多线程SBGGR10图像数据解包主函数
 */
int unpack_sbggr10_image(const uint8_t *raw_data, size_t raw_size, 
                        uint16_t *output_pixels, size_t num_pixels) {
    if (!raw_data || !output_pixels || raw_size == 0) {
        return -1;
    }
    
    // 验证数据大小（必须是5的倍数）
    if (raw_size % 5 != 0) {
        printf("Error: RAW data size (%zu) must be multiple of 5\n", raw_size);
        return -1;
    }
    
    size_t expected_pixels = raw_size / 5 * 4;
    if (num_pixels < expected_pixels) {
        printf("Error: Output buffer too small (%zu < %zu)\n", num_pixels, expected_pixels);
        return -1;
    }
    
    // 获取CPU核心数，决定线程数量
    int cpu_cores = get_cpu_cores();
    int num_threads = (raw_size < MIN_CHUNK_SIZE) ? 1 : cpu_cores;
    
    if (num_threads > 8) num_threads = 8;  // 限制最大线程数
    
    // 单线程处理小数据量
    if (num_threads == 1) {
        struct unpack_task task = {
            .raw_data = raw_data,
            .output_data = output_pixels,
            .start_offset = 0,
            .end_offset = raw_size,
            .thread_id = 0
        };
        unpack_worker_thread(&task);
        return 0;
    }
    
    // 多线程处理
    struct unpack_task tasks[8];
    size_t chunk_size = raw_size / num_threads;
    chunk_size = (chunk_size / 5) * 5;  // 确保5字节对齐
    
#ifdef _WIN32
    HANDLE threads[8];
#else
    pthread_t threads[8];
#endif
    
    uint64_t start_time = get_time_ns();
    
    // 创建工作线程
    for (int i = 0; i < num_threads; i++) {
        tasks[i].raw_data = raw_data;
        tasks[i].output_data = output_pixels;
        tasks[i].start_offset = i * chunk_size;
        tasks[i].end_offset = (i == num_threads - 1) ? raw_size : (i + 1) * chunk_size;
        tasks[i].thread_id = i;
        
#ifdef _WIN32
        threads[i] = (HANDLE)_beginthreadex(NULL, 0, unpack_worker_thread, &tasks[i], 0, NULL);
        if (threads[i] == 0) {
            return -1;
        }
#else
        if (pthread_create(&threads[i], NULL, unpack_worker_thread, &tasks[i]) != 0) {
            return -1;
        }
#endif
    }
    
    // 等待所有线程完成
    for (int i = 0; i < num_threads; i++) {
#ifdef _WIN32
        WaitForSingleObject(threads[i], INFINITE);
        CloseHandle(threads[i]);
#else
        pthread_join(threads[i], NULL);
#endif
    }
    
    uint64_t end_time = get_time_ns();
    double elapsed_ms = (end_time - start_time) / 1000000.0;
    double throughput = (raw_size / 1024.0 / 1024.0) / (elapsed_ms / 1000.0);
    
    static int call_count = 0;
    call_count++;
    if (call_count <= 3 || call_count % 50 == 0) {
        printf("SBGGR10 unpacking: %.1f ms, %.1f MB/s\n", elapsed_ms, throughput);
    }
    
    return 0;
}

// ========================== 图像数据处理函数 ==========================

/**
 * @brief 保存接收到的图像帧数据到文件（可选）
 */
int save_frame(const uint8_t* data, size_t size, uint32_t frame_id,
               uint32_t width, uint32_t height, uint32_t pixfmt,
               int enable_conversion, const char* output_dir)
{
    // 如果没有指定输出目录，则不保存文件
    if (!output_dir) {
        return 0;  // 内存模式，直接返回成功
    }

    char filename[MAX_FILENAME_LEN];
    int ret = 0;

    // 根据像素格式确定文件扩展名
    const char* ext = "raw";
    if (pixfmt == 0x30314742) {  // V4L2_PIX_FMT_SBGGR10
        ext = "BG10";
    }

    // 保存原始RAW数据
    snprintf(filename, sizeof(filename), "%s/frame_%06d_%dx%d.%s",
             output_dir, frame_id, width, height, ext);

    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        printf("Failed to open output file: %s\n", filename);
        return -1;
    }

    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);

    if (written != size) {
        printf("Warning: wrote %zu bytes instead of %zu\n", written, size);
        ret = -1;
    }

    // 对于SBGGR10格式，进行数据解包并保存处理后的数据
    if (enable_conversion && pixfmt == 0x30314742 && size % 5 == 0) {
        size_t num_pixels = size / 5 * 4;
        uint16_t* unpacked_pixels = NULL;
        
        // 尝试使用预分配的内存池
        if (g_unpack_buffer && num_pixels <= g_buffer_size) {
            unpacked_pixels = g_unpack_buffer;
        } else {
            unpacked_pixels = (uint16_t*)malloc(num_pixels * sizeof(uint16_t));
        }
        
        if (unpacked_pixels) {
            if (unpack_sbggr10_image(data, size, unpacked_pixels, num_pixels) == 0) {
                // 保存解包后的16位数据
                snprintf(filename, sizeof(filename), "%s/frame_%06d_%dx%d_unpacked.raw",
                        output_dir, frame_id, width, height);
                
                FILE* fp_unpacked = fopen(filename, "wb");
                if (fp_unpacked) {
                    size_t unpacked_size = num_pixels * sizeof(uint16_t);
                    size_t written_unpacked = fwrite(unpacked_pixels, 1, unpacked_size, fp_unpacked);
                    fclose(fp_unpacked);
                    
                    if (written_unpacked != unpacked_size) {
                        printf("Warning: unpacked write incomplete (%zu/%zu bytes)\n", 
                               written_unpacked, unpacked_size);
                        ret = -1;
                    }
                } else {
                    printf("Failed to open unpacked output file: %s\n", filename);
                    ret = -1;
                }
            } else {
                printf("Failed to unpack SBGGR10 data\n");
                ret = -1;
            }
            
            // 只有不是预分配缓冲区时才需要释放
            if (unpacked_pixels != g_unpack_buffer) {
                free(unpacked_pixels);
            }
        } else {
            printf("Failed to allocate memory for unpacked data (%zu pixels)\n", num_pixels);
            ret = -1;
        }
    }

    return ret;
}

/**
 * @brief 仅在内存中处理图像帧数据（不保存文件）
 */
int process_frame_memory_only(const uint8_t* data, size_t size, uint32_t frame_id,
                              uint32_t pixfmt, int enable_conversion)
{
    // 仅处理SBGGR10格式转换（如果启用）
    if (enable_conversion && pixfmt == 0x30314742 && size % 5 == 0) {
        size_t num_pixels = size / 5 * 4;
        uint16_t* unpacked_pixels = NULL;
        
        // 尝试使用预分配的内存池
        if (g_unpack_buffer && num_pixels <= g_buffer_size) {
            unpacked_pixels = g_unpack_buffer;
        } else {
            unpacked_pixels = (uint16_t*)malloc(num_pixels * sizeof(uint16_t));
        }
        
        if (unpacked_pixels) {
            if (unpack_sbggr10_image(data, size, unpacked_pixels, num_pixels) == 0) {
                // 数据已在内存中处理完成，无需保存文件
                static int process_count = 0;
                process_count++;
                if (process_count <= 3 || process_count % 100 == 0) {
                    printf("Frame %d: SBGGR10 converted in memory (%zu pixels)\n", 
                           frame_id, num_pixels);
                }
            } else {
                printf("Failed to unpack SBGGR10 data in memory\n");
                if (unpacked_pixels != g_unpack_buffer) {
                    free(unpacked_pixels);
                }
                return -1;
            }
            
            // 只有不是预分配缓冲区时才需要释放
            if (unpacked_pixels != g_unpack_buffer) {
                free(unpacked_pixels);
            }
        } else {
            printf("Failed to allocate memory for processing (%zu pixels)\n", num_pixels);
            return -1;
        }
    }
    
    return 0;
}

/**
 * @brief 打印接收到的帧信息
 */
void print_frame_info(const struct frame_header* header)
{
    printf("Frame %d: %dx%d, pixfmt=0x%08x (%c%c%c%c), size=%d bytes, ",
           header->frame_id, header->width, header->height, header->pixfmt,
           (header->pixfmt >> 0) & 0xFF, (header->pixfmt >> 8) & 0xFF,
           (header->pixfmt >> 16) & 0xFF, (header->pixfmt >> 24) & 0xFF,
           header->size);

    double timestamp_sec = header->timestamp / 1000000000.0;
    printf("timestamp=%.3fs\n", timestamp_sec);
}

// ========================== 性能统计函数 ==========================

/**
 * @brief 更新传输性能统计信息
 */
void update_stats(uint32_t frame_size)
{
    uint64_t current_time = get_time_ns();

    if (stats.start_time == 0) {
        stats.start_time = current_time;
    }

    stats.frames_received++;
    stats.bytes_received += frame_size;

    // 计算FPS
    if (stats.last_frame_time > 0) {
        uint64_t elapsed = current_time - stats.start_time;
        if (elapsed > 0) {
            stats.avg_fps = (double)stats.frames_received * 1000000000.0 / elapsed;
        }
    }

    stats.last_frame_time = current_time;
}

/**
 * @brief 打印最终统计信息
 */
void print_stats(void)
{
    uint64_t current_time = get_time_ns();
    double elapsed_sec = (current_time - stats.start_time) / 1000000000.0;
    double mbps = (stats.bytes_received / 1024.0 / 1024.0) / elapsed_sec;

    printf("\n=== Statistics ===\n");
    printf("Frames received: %d\n", stats.frames_received);
    printf("Bytes received: %d (%.2f MB)\n", stats.bytes_received,
           stats.bytes_received / 1024.0 / 1024.0);
    printf("Elapsed time: %.2f seconds\n", elapsed_sec);
    printf("Average FPS: %.2f\n", stats.avg_fps);
    printf("Data rate: %.2f MB/s\n", mbps);
}

// ========================== 主接收循环 ==========================

/**
 * @brief 图像数据接收主循环
 */
int receive_loop(socket_t sock, const struct client_config* config)
{
    uint8_t* frame_buffer = NULL;
    size_t buffer_size = 0;

    printf("Starting receive loop (Ctrl+C to stop)...\n");
    if (config->enable_save) {
        printf("Frames will be saved to: %s\n", config->output_dir);
        printf("SBGGR10 conversion: %s\n", config->enable_conversion ? "Enabled" : "Disabled");
    } else {
        printf("Memory-only mode: No files will be saved\n");
        printf("SBGGR10 processing: %s\n", config->enable_conversion ? "In-memory conversion" : "No processing");
    }

    while (running) {
        struct frame_header header;

        // 接收帧头
        if (recv_full(sock, &header, sizeof(header)) < 0) {
            break;
        }

        // 验证魔数
        if (header.magic != 0xDEADBEEF) {
            printf("Invalid frame magic: 0x%08x\n", header.magic);
            break;
        }

        // 检查帧大小合理性
        if (header.size == 0 || header.size > 50 * 1024 * 1024) {
            printf("Invalid frame size: %d\n", header.size);
            break;
        }

        // 重新分配缓冲区（如果需要）
        if (header.size > buffer_size) {
            free(frame_buffer);
            frame_buffer = malloc(header.size);
            if (!frame_buffer) {
                printf("Failed to allocate %d bytes for frame buffer\n", header.size);
                break;
            }
            buffer_size = header.size;
        }

        // 接收帧数据
        if (recv_full(sock, frame_buffer, header.size) < 0) {
            break;
        }

        // 打印帧信息
        print_frame_info(&header);

        // 处理帧（保存或仅内存处理）
        if (header.frame_id % config->save_interval == 0) {
            if (config->enable_save) {
                // 文件保存模式
                if (save_frame(frame_buffer, header.size, header.frame_id,
                              header.width, header.height, header.pixfmt,
                              config->enable_conversion, config->output_dir) == 0) {
                    if (config->enable_conversion && header.pixfmt == 0x30314742) {
                        printf("  -> Saved RAW + unpacked files\n");
                    } else {
                        printf("  -> Saved RAW file\n");
                    }
                }
            } else {
                // 仅内存处理模式
                if (process_frame_memory_only(frame_buffer, header.size, header.frame_id,
                                            header.pixfmt, config->enable_conversion) == 0) {
                    if (config->enable_conversion && header.pixfmt == 0x30314742) {
                        printf("  -> Processed in memory (converted)\n");
                    } else {
                        printf("  -> Processed in memory (raw)\n");
                    }
                }
            }
        }

        // 更新统计
        update_stats(header.size);

        // 每100帧显示一次统计
        if (stats.frames_received % 100 == 0) {
            printf("Received %d frames, avg FPS: %.2f\n",
                   stats.frames_received, stats.avg_fps);
        }
    }

    free(frame_buffer);
    return 0;
}
