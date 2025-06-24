/**
 * @file v4l2_usb_pc.h
 * @brief V4L2图像流跨平台PC客户端头文件
 *
 * 定义了PC客户端的主要接口、数据结构和常量定义。
 * 支持Windows、Linux和macOS多平台。
 *
 * @author Development Team
 * @date 2025-06-24
 * @version 2.0
 */

#ifndef V4L2_USB_PC_H
#define V4L2_USB_PC_H

// ========================== 标准库头文件 ==========================

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ========================== 跨平台兼容性处理 ==========================

#ifdef _WIN32
// Windows平台专用头文件和定义
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <process.h>
#define ssize_t int
typedef int socklen_t;
typedef SOCKET socket_t;
#define INVALID_SOCKET_FD INVALID_SOCKET
#define close_socket      closesocket
#elif defined(__APPLE__)
// macOS平台专用头文件和定义
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <sys/sysctl.h>
typedef int socket_t;
#define INVALID_SOCKET_FD -1
#define close_socket      close
#else
// Linux平台专用头文件和定义
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
typedef int socket_t;
#define INVALID_SOCKET_FD -1
#define close_socket      close
#endif

// SIMD指令集支持
#ifdef __SSE2__
#include <emmintrin.h>
#endif
#ifdef __AVX2__
#include <immintrin.h>
#endif

// ========================== 系统配置常量 ==========================

/** @brief 默认嵌入式服务器IP地址 */
#define DEFAULT_SERVER_IP "172.32.0.93"

/** @brief 默认TCP服务器端口 */
#define DEFAULT_PORT 8888

/** @brief 图像文件输出目录 */
#define OUTPUT_DIR "./received_frames"

/** @brief 文件名最大长度 */
#define MAX_FILENAME_LEN 256

/** @brief 网络接收超时时间，单位：秒 */
#define RECV_TIMEOUT_SEC 10

/** @brief 解包线程的最小数据块大小，单位：字节 */
#define MIN_CHUNK_SIZE (1024 * 1024)  // 1MB

// ========================== 通信协议定义 ==========================

/**
 * @struct frame_header
 * @brief 图像帧头部结构（与嵌入式端完全一致）
 *
 * 该结构体定义了嵌入式端和PC端之间的通信协议格式。
 */
struct frame_header
{
    uint32_t magic;       /**< 协议魔数：0xDEADBEEF */
    uint32_t frame_id;    /**< 帧序号，从0开始递增 */
    uint32_t width;       /**< 图像宽度，像素为单位 */
    uint32_t height;      /**< 图像高度，像素为单位 */
    uint32_t pixfmt;      /**< 像素格式，V4L2格式代码 */
    uint32_t size;        /**< 图像数据大小，字节为单位 */
    uint64_t timestamp;   /**< 嵌入式端时间戳，纳秒为单位 */
    uint32_t reserved[2]; /**< 保留字段，用于未来扩展 */
} __attribute__((packed));

/**
 * @struct stats
 * @brief 传输性能统计信息结构
 */
struct stats
{
    uint32_t frames_received; /**< 已接收的帧总数 */
    uint32_t bytes_received;  /**< 已接收的字节总数 */
    uint64_t start_time;      /**< 程序开始时间，纳秒 */
    uint64_t last_frame_time; /**< 最后一帧接收时间，纳秒 */
    double avg_fps;           /**< 平均帧率，帧/秒 */
};

/**
 * @struct unpack_task
 * @brief 图像解包任务结构体
 */
struct unpack_task {
    const uint8_t *raw_data;    /**< 输入RAW数据指针 */
    uint16_t *output_data;      /**< 输出16位像素数据指针 */
    size_t start_offset;        /**< 处理的起始偏移量（5字节对齐） */
    size_t end_offset;          /**< 处理的结束偏移量 */
    int thread_id;              /**< 线程ID */
};

/**
 * @struct client_config
 * @brief 客户端配置结构体
 */
struct client_config {
    const char* server_ip;       /**< 服务器IP地址 */
    int port;                    /**< 服务器端口 */
    const char* output_dir;      /**< 输出目录 */
    int enable_conversion;       /**< 是否启用SBGGR10转换 */
    int save_interval;           /**< 保存间隔 */
};

// ========================== 全局变量声明 ==========================

extern volatile int running;
extern socket_t sock_fd;
extern struct stats stats;
extern uint16_t* g_unpack_buffer;
extern size_t g_buffer_size;

// ========================== 函数声明 ==========================

// 跨平台工具函数
int get_cpu_cores(void);
uint64_t get_time_ns(void);
int create_directory(const char* path);
void sleep_ms(int ms);

// 网络初始化函数
int init_network(void);
void cleanup_network(void);
socket_t connect_to_server(const char* ip, int port);
int recv_full(socket_t sock, void* buffer, size_t size);

// 文件系统管理函数
int create_output_dir(const char* dir);

// 信号处理函数
#ifdef _WIN32
BOOL WINAPI console_handler(DWORD signal);
#else
void signal_handler(int sig);
#endif

// 图像数据处理函数
int save_frame(const uint8_t* data, size_t size, uint32_t frame_id,
               uint32_t width, uint32_t height, uint32_t pixfmt,
               int enable_conversion);
void print_frame_info(const struct frame_header* header);

// SBGGR10解包函数
void unpack_sbggr10_scalar(const uint8_t raw_bytes[5], uint16_t pixels[4]);
#ifdef __AVX2__
void unpack_sbggr10_avx2(const uint8_t *raw_data, uint16_t *output, size_t num_blocks);
#endif
#ifdef _WIN32
unsigned int __stdcall unpack_worker_thread(void *arg);
#else
void* unpack_worker_thread(void *arg);
#endif
int unpack_sbggr10_image(const uint8_t *raw_data, size_t raw_size, 
                        uint16_t *output_pixels, size_t num_pixels);

// 性能统计函数
void update_stats(uint32_t frame_size);
void print_stats(void);

// 主接收循环
int receive_loop(socket_t sock, const struct client_config* config);

// 命令行界面函数
void print_usage(const char* prog_name);
int parse_arguments(int argc, char* argv[], struct client_config* config);

// 内存管理函数
void init_memory_pool(void);
void cleanup_memory_pool(void);

#endif // V4L2_USB_PC_H
