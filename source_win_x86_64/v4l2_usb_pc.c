/**
 * @file v4l2_usb_pc.c
 * @brief V4L2图像流跨平台PC客户端
 *
 * 本程序是与嵌入式端v4l2_usb.c配套的PC客户端，负责接收和保存图像数据。
 * 主要功能包括：
 * - 跨平台网络通信（Windows/Linux）
 * - 与嵌入式端建立TCP连接
 * - 接收并解析自定义图像帧协议
 * - 将RAW图像数据保存到本地文件
 * - 实时统计传输性能数据
 *
 * 设计架构：
 * [嵌入式端] v4l2_usb.c (图像采集+TCP服务器)
 *     ↓ TCP Socket通信
 * [PC客户端] v4l2_usb_pc.c (图像接收+文件保存)
 *
 * @author Development Team
 * @date 2025-06-23
 * @version 1.0
 */

// ========================== 跨平台兼容性处理 ==========================

#ifdef _WIN32
// Windows平台专用头文件和定义
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
// 注意：使用编译器链接选项而不是pragma
#define ssize_t int                      /**< Windows下的ssize_t类型定义 */
typedef int socklen_t;                   /**< Windows下的socklen_t类型定义 */
typedef SOCKET socket_t;                 /**< Windows下的socket类型 */
#define INVALID_SOCKET_FD INVALID_SOCKET /**< Windows下的无效socket值 */
#define close_socket      closesocket    /**< Windows下的socket关闭函数 */
#else
// Linux/Unix平台专用头文件和定义
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef int socket_t; /**< Linux下的socket类型 */
#define INVALID_SOCKET_FD -1    /**< Linux下的无效socket值 */
#define close_socket      close /**< Linux下的socket关闭函数 */
#endif

// ========================== 标准库头文件 ==========================

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

// ========================== 系统配置常量 ==========================

/** @brief 默认嵌入式服务器IP地址（与v4l2_usb.c中的DEFAULT_SERVER_IP对应） */
#define DEFAULT_SERVER_IP "172.32.0.93"

/** @brief 默认TCP服务器端口（与v4l2_usb.c中的DEFAULT_PORT对应） */
#define DEFAULT_PORT 8888

/** @brief 图像文件输出目录 */
#define OUTPUT_DIR "./received_frames"

/** @brief 文件名最大长度 */
#define MAX_FILENAME_LEN 256

/** @brief 网络接收超时时间，单位：秒 */
#define RECV_TIMEOUT_SEC 10

// ========================== 通信协议定义 ==========================

/**
 * @struct frame_header
 * @brief 图像帧头部结构（与嵌入式端完全一致）
 *
 * 该结构体定义了嵌入式端和PC端之间的通信协议格式。
 * 与v4l2_usb.c中的frame_header结构体必须保持完全一致，
 * 包括字段顺序、数据类型和内存对齐方式。
 *
 * 协议流程：
 * 1. 嵌入式端发送frame_header（32字节）
 * 2. 嵌入式端分块发送图像数据
 * 3. PC端接收header，验证魔数
 * 4. PC端根据size字段接收完整图像数据
 */
struct frame_header
{
    uint32_t magic;       /**< 协议魔数：0xDEADBEEF，用于验证数据完整性 */
    uint32_t frame_id;    /**< 帧序号，从0开始递增，用于跟踪丢帧 */
    uint32_t width;       /**< 图像宽度，像素为单位 */
    uint32_t height;      /**< 图像高度，像素为单位 */
    uint32_t pixfmt;      /**< 像素格式，V4L2格式代码 */
    uint32_t size;        /**< 图像数据大小，字节为单位 */
    uint64_t timestamp;   /**< 嵌入式端时间戳，纳秒为单位 */
    uint32_t reserved[2]; /**< 保留字段，用于未来扩展 */
} __attribute__((packed));

// ========================== 全局状态变量 ==========================

/** @brief 程序运行状态标志，0表示停止，1表示运行 */
volatile int running = 1;

/** @brief 主TCP连接socket文件描述符 */
socket_t sock_fd = INVALID_SOCKET_FD;

/**
 * @struct stats
 * @brief 传输性能统计信息结构
 *
 * 用于记录和计算图像传输的各项性能指标，
 * 帮助评估网络传输质量和系统性能。
 */
struct stats
{
    uint32_t frames_received; /**< 已接收的帧总数 */
    uint32_t bytes_received;  /**< 已接收的字节总数 */
    uint64_t start_time;      /**< 程序开始时间，纳秒 */
    uint64_t last_frame_time; /**< 最后一帧接收时间，纳秒 */
    double avg_fps;           /**< 平均帧率，帧/秒 */
} stats = {0};

// ========================== 跨平台工具函数 ==========================

/**
 * @brief 获取高精度时间戳（跨平台实现）
 *
 * 提供跨平台的纳秒级时间戳获取功能。
 * Windows使用QueryPerformanceCounter，Linux使用clock_gettime。
 * 与嵌入式端的get_time_ns()函数功能相同，确保时间戳的一致性。
 *
 * @return 返回纳秒级时间戳
 */
static inline uint64_t get_time_ns()
{
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)(counter.QuadPart * 1000000000ULL / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(1, &ts);  // 1 = CLOCK_MONOTONIC
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

/**
 * @brief 跨平台目录创建函数
 *
 * 在不同操作系统上创建目录，处理平台差异。
 * Windows使用CreateDirectoryA，Linux使用mkdir。
 *
 * @param path 目录路径
 * @return 成功返回0，失败返回-1
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
 *
 * 提供跨平台的毫秒级延时功能。
 * Windows使用Sleep，Linux使用usleep。
 *
 * @param ms 延时毫秒数
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
 *
 * 处理Windows系统的控制台事件，如Ctrl+C和窗口关闭。
 * 设置running标志为0，触发程序优雅退出。
 *
 * @param signal 接收到的控制台信号类型
 * @return TRUE表示信号已处理，FALSE表示传递给下一个处理器
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
 *
 * 处理SIGINT、SIGTERM等系统信号，实现程序的优雅退出。
 * 与嵌入式端的signal_handler()函数功能类似。
 *
 * @param sig 接收到的信号编号
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
 *
 * Windows需要调用WSAStartup初始化Winsock库，
 * Linux/Unix系统不需要特殊初始化。
 * 与嵌入式端的socket创建形成配套的客户端初始化。
 *
 * @return 成功返回0，失败返回-1
 */
int init_network()
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
 *
 * Windows需要调用WSACleanup清理Winsock库，
 * Linux/Unix系统不需要特殊清理。
 */
void cleanup_network()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

// ========================== 文件系统管理函数 ==========================

/**
 * @brief 创建图像输出目录（跨平台）
 *
 * 检查指定目录是否存在，如不存在则创建。
 * 用于保存从嵌入式端接收到的图像文件。
 *
 * Windows使用FindFirstFileA检查目录存在性，
 * Linux使用stat系统调用检查。
 *
 * @param dir 要创建的目录路径
 * @return 成功返回0，失败返回-1
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
 *
 * TCP socket的recv()可能不会一次接收完所有数据，
 * 该函数确保接收到指定大小的完整数据。
 * 这对于接收嵌入式端发送的帧数据非常重要。
 *
 * 与嵌入式端的send_frame()函数配套使用：
 * - 嵌入式端分块发送数据（CHUNK_SIZE=64KB）
 * - PC端循环接收直到获得完整数据
 *
 * @param sock socket文件描述符
 * @param buffer 接收数据的缓冲区
 * @param size 要接收的字节数
 * @return 成功返回0，失败返回-1
 */
int recv_full(socket_t sock, void* buffer, size_t size)
{
    size_t received = 0;
    uint8_t* ptr    = (uint8_t*)buffer;

    while (received < size && running)
    {
        ssize_t result =
            recv(sock, (char*)(ptr + received), (int)(size - received), 0);

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
 *
 * 创建TCP socket并连接到嵌入式端的服务器。
 * 与嵌入式端v4l2_usb.c中的create_server()函数配套：
 * - 嵌入式端: create_server() -> listen() -> accept()
 * - PC端: connect_to_server() -> connect()
 *
 * 配置接收超时以避免无限等待，提高程序的健壮性。
 *
 * @param ip 服务器IP地址（默认172.32.0.93）
 * @param port 服务器端口（默认8888）
 * @return 成功返回socket文件描述符，失败返回INVALID_SOCKET_FD
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
    DWORD timeout = RECV_TIMEOUT_SEC * 1000;  // Windows uses milliseconds
    if (setsockopt(
            sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) <
        0)
    {
        printf("setsockopt failed: %d\n", WSAGetLastError());
        close_socket(sock);
        return INVALID_SOCKET_FD;
    }
#else
    struct timeval timeout;
    timeout.tv_sec  = RECV_TIMEOUT_SEC;
    timeout.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) <
        0)
    {
        perror("setsockopt failed");
        close_socket(sock);
        return INVALID_SOCKET_FD;
    }
#endif

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(port);

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

// ========================== 图像数据处理函数 ==========================

/**
 * @brief 保存接收到的图像帧数据到文件
 *
 * 将从嵌入式端接收到的RAW图像数据保存为本地文件。
 * 文件命名格式：frame_XXXXXX_WIDTHxHEIGHT.EXT
 * 其中EXT根据像素格式确定（如BG10表示10位BGGR格式）。
 *
 * 与嵌入式端的图像采集流程对应：
 * 嵌入式端：V4L2采集 -> 内存缓冲区 -> TCP发送
 * PC端：TCP接收 -> 内存缓冲区 -> 文件保存
 *
 * @param data 图像数据指针
 * @param size 图像数据大小
 * @param frame_id 帧序号
 * @param width 图像宽度
 * @param height 图像高度
 * @param pixfmt V4L2像素格式代码
 * @return 成功返回0，失败返回-1
 */
int save_frame(const uint8_t* data,
               size_t size,
               uint32_t frame_id,
               uint32_t width,
               uint32_t height,
               uint32_t pixfmt)
{
    char filename[MAX_FILENAME_LEN];

    // 根据像素格式确定文件扩展名
    const char* ext = "raw";
    if (pixfmt == 0x30314742)
    {  // V4L2_PIX_FMT_SBGGR10
        ext = "BG10";
    }

    snprintf(filename,
             sizeof(filename),
             "%s/frame_%06d_%dx%d.%s",
             OUTPUT_DIR,
             frame_id,
             width,
             height,
             ext);

    FILE* fp = fopen(filename, "wb");
    if (!fp)
    {
        printf("Failed to open output file: %s\n", filename);
        return -1;
    }

    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);

    if (written != size)
    {
        printf("Warning: wrote %zu bytes instead of %zu\n", written, size);
        return -1;
    }

    return 0;
}

/**
 * @brief 打印接收到的帧信息
 *
 * 解析并显示帧头中的各项信息，帮助调试和监控。
 * 显示像素格式的四字符代码（FourCC），便于识别格式类型。
 *
 * @param header 帧头结构体指针
 */
void print_frame_info(const struct frame_header* header)
{
    printf("Frame %d: %dx%d, pixfmt=0x%08x (%c%c%c%c), size=%d bytes, ",
           header->frame_id,
           header->width,
           header->height,
           header->pixfmt,
           (header->pixfmt >> 0) & 0xFF,
           (header->pixfmt >> 8) & 0xFF,
           (header->pixfmt >> 16) & 0xFF,
           (header->pixfmt >> 24) & 0xFF,
           header->size);

    // 计算时间戳
    double timestamp_sec = header->timestamp / 1000000000.0;
    printf("timestamp=%.3fs\n", timestamp_sec);
}

// ========================== 性能统计函数 ==========================

/**
 * @brief 更新传输性能统计信息
 *
 * 记录每帧接收的统计数据，计算平均帧率和数据传输率。
 * 与嵌入式端的性能统计形成对比，用于评估端到端性能。
 *
 * @param frame_size 当前帧的数据大小
 */
void update_stats(uint32_t frame_size)
{
    uint64_t current_time = get_time_ns();

    if (stats.start_time == 0)
    {
        stats.start_time = current_time;
    }

    stats.frames_received++;
    stats.bytes_received += frame_size;

    // 计算FPS
    if (stats.last_frame_time > 0)
    {
        uint64_t elapsed = current_time - stats.start_time;
        if (elapsed > 0)
        {
            stats.avg_fps =
                (double)stats.frames_received * 1000000000.0 / elapsed;
        }
    }

    stats.last_frame_time = current_time;
}

/**
 * @brief 打印最终统计信息
 *
 * 在程序结束时显示完整的传输性能报告，
 * 包括总帧数、数据量、传输时间、平均帧率和数据传输率。
 */
void print_stats()
{
    uint64_t current_time = get_time_ns();
    double elapsed_sec    = (current_time - stats.start_time) / 1000000000.0;
    double mbps = (stats.bytes_received / 1024.0 / 1024.0) / elapsed_sec;

    printf("\n=== Statistics ===\n");
    printf("Frames received: %d\n", stats.frames_received);
    printf("Bytes received: %d (%.2f MB)\n",
           stats.bytes_received,
           stats.bytes_received / 1024.0 / 1024.0);
    printf("Elapsed time: %.2f seconds\n", elapsed_sec);
    printf("Average FPS: %.2f\n", stats.avg_fps);
    printf("Data rate: %.2f MB/s\n", mbps);
}

// ========================== 主接收循环 ==========================

/**
 * @brief 图像数据接收主循环
 *
 * 程序的核心处理循环，与嵌入式端的capture_loop()形成完整的数据流：
 *
 * 嵌入式端流程：
 * capture_loop() -> dequeue_buffer_mp() -> send_frame() -> TCP发送
 *
 * PC端流程：
 * receive_loop() -> recv_full() -> save_frame() -> 文件保存
 *
 * 协议处理步骤：
 * 1. 接收固定大小的frame_header（32字节）
 * 2. 验证魔数0xDEADBEEF，确保数据同步
 * 3. 根据header.size动态分配缓冲区
 * 4. 接收完整的图像数据
 * 5. 保存到文件并更新统计信息
 *
 * @param sock TCP连接socket
 * @return 正常退出返回0
 */
int receive_loop(socket_t sock)
{
    uint8_t* frame_buffer = NULL;
    size_t buffer_size    = 0;
    int save_enabled      = 1;
    int save_interval     = 1;  // 每1帧保存一次

    printf("Starting receive loop (Ctrl+C to stop)...\n");
    printf("Frames will be saved to: %s\n", OUTPUT_DIR);

    while (running)
    {
        struct frame_header header;

        // 接收帧头
        if (recv_full(sock, &header, sizeof(header)) < 0)
        {
            break;
        }

        // 验证魔数
        if (header.magic != 0xDEADBEEF)
        {
            printf("Invalid frame magic: 0x%08x\n", header.magic);
            break;
        }

        // 检查帧大小合理性
        if (header.size == 0 || header.size > 50 * 1024 * 1024)
        {  // 最大50MB
            printf("Invalid frame size: %d\n", header.size);
            break;
        }

        // 重新分配缓冲区（如果需要）
        if (header.size > buffer_size)
        {
            free(frame_buffer);
            frame_buffer = malloc(header.size);
            if (!frame_buffer)
            {
                printf("Failed to allocate %d bytes for frame buffer\n",
                       header.size);
                break;
            }
            buffer_size = header.size;
            printf("Allocated %zu bytes frame buffer\n", buffer_size);
        }

        // 接收帧数据
        if (recv_full(sock, frame_buffer, header.size) < 0)
        {
            break;
        }

        // 打印帧信息
        print_frame_info(&header);

        // 保存帧（根据设置）
        if (save_enabled && (header.frame_id % save_interval == 0))
        {
            if (save_frame(frame_buffer,
                           header.size,
                           header.frame_id,
                           header.width,
                           header.height,
                           header.pixfmt) == 0)
            {
                printf("  -> Saved to file\n");
            }
        }

        // 更新统计
        update_stats(header.size);

        // 每100帧显示一次统计
        if (stats.frames_received % 100 == 0)
        {
            printf("Received %d frames, avg FPS: %.2f\n",
                   stats.frames_received,
                   stats.avg_fps);
        }
    }

    free(frame_buffer);
    return 0;
}

// ========================== 命令行界面函数 ==========================

/**
 * @brief 打印程序使用说明
 *
 * 显示命令行参数的详细说明和使用示例。
 *
 * @param prog_name 程序名称
 */
void print_usage(const char* prog_name)
{
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("Options:\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -s, --server IP     Server IP address (default: %s)\n",
           DEFAULT_SERVER_IP);
    printf("  -p, --port PORT     Server port (default: %d)\n", DEFAULT_PORT);
    printf("  -o, --output DIR    Output directory (default: %s)\n",
           OUTPUT_DIR);
    printf("\nExample:\n");
    printf("  %s -s 172.32.0.93 -p 8888 -o ./frames\n", prog_name);
    printf(
        "\nNote: On Windows, use forward slashes or double backslashes for "
        "paths\n");
    printf("  Good: ./frames or .\\\\frames\n");
    printf("  Bad:  .\\frames\n");
}

// ========================== 程序主函数 ==========================

/**
 * @brief 程序入口点
 *
 * PC客户端的主函数，负责整个程序的初始化和协调工作。
 * 与嵌入式端v4l2_usb.c的main()函数形成完整的系统架构：
 *
 * 系统架构对比：
 * ┌─────────────────┐     TCP Socket     ┌─────────────────┐
 * │  嵌入式端       │ ←──────────────→ │  PC客户端       │
 * │  v4l2_usb.c     │                   │  v4l2_usb_pc.c  │
 * ├─────────────────┤                   ├─────────────────┤
 * │ V4L2设备初始化   │                   │ 网络初始化       │
 * │ 缓冲区申请       │                   │ 目录创建         │
 * │ 视频流启动       │                   │ Socket连接       │
 * │ TCP服务器创建    │                   │ 数据接收循环     │
 * │ 图像采集循环     │                   │ 文件保存         │
 * │ 网络发送线程     │                   │ 性能统计         │
 * └─────────────────┘                   └─────────────────┘
 *
 * 主函数流程：
 * 1. 解析命令行参数（服务器IP、端口、输出目录）
 * 2. 初始化跨平台网络子系统
 * 3. 设置信号处理器
 * 4. 创建输出目录
 * 5. 连接到嵌入式端TCP服务器
 * 6. 执行图像接收主循环
 * 7. 清理资源并显示统计信息
 *
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 程序退出码，成功返回0
 */
int main(int argc, char* argv[])
{
    const char* server_ip  = DEFAULT_SERVER_IP;
    int port               = DEFAULT_PORT;
    const char* output_dir = OUTPUT_DIR;

    // 解析命令行参数
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            print_usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--server") == 0)
        {
            if (++i < argc)
            {
                server_ip = argv[i];
            }
            else
            {
                printf("Error: --server requires an IP address\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0)
        {
            if (++i < argc)
            {
                port = atoi(argv[i]);
                if (port <= 0 || port > 65535)
                {
                    printf("Error: Invalid port number\n");
                    return 1;
                }
            }
            else
            {
                printf("Error: --port requires a port number\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0)
        {
            if (++i < argc)
            {
                output_dir = argv[i];
            }
            else
            {
                printf("Error: --output requires a directory path\n");
                return 1;
            }
        }
        else
        {
            printf("Error: Unknown option %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    printf("V4L2 USB RAW Image Receiver (Cross-Platform PC Client)\n");
    printf("=====================================================\n");
    printf("Server: %s:%d\n", server_ip, port);
    printf("Output: %s\n", output_dir);

    // 初始化网络
    if (init_network() < 0)
    {
        return 1;
    }

    // 设置信号处理
#ifdef _WIN32
    SetConsoleCtrlHandler(console_handler, TRUE);
#else
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#endif

    // 创建输出目录
    if (create_output_dir(output_dir) < 0)
    {
        cleanup_network();
        return 1;
    }

    // 连接到服务器
    sock_fd = connect_to_server(server_ip, port);
    if (sock_fd == INVALID_SOCKET_FD)
    {
        cleanup_network();
        return 1;
    }

    // 主接收循环
    int result = receive_loop(sock_fd);

    // 清理
    close_socket(sock_fd);
    cleanup_network();

    // 打印最终统计
    print_stats();

    printf("Program terminated\n");
    return result;
}
