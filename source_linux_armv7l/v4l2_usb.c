/**
 * @file v4l2_usb.c
 * @brief V4L2多平面图像采集与USB传输系统
 *
 * 本程序实现了基于V4L2的多平面图像采集系统，专为Luckfox Pico Mini B设备设计。
 * 主要功能包括：
 * - 通过V4L2接口采集RAW格式的图像数据
 * - 使用多线程架构实现实时图像流传输
 * - 通过TCP Socket将图像数据发送给客户端
 * - 支持多平面缓冲区管理和内存映射
 *
 * @author Development Team
 * @date 2025-06-23
 * @version 1.0
 */

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

// ========================== 系统配置常量 ==========================

/** @brief 图像宽度，单位：像素 */
#define WIDTH 1920

/** @brief 图像高度，单位：像素 */
#define HEIGHT 1080

/** @brief 像素格式：10位BGGR原始数据格式 */
#define PIXELFORMAT V4L2_PIX_FMT_SBGGR10

/** @brief V4L2缓冲区数量，减少内存占用 */
#define BUFFER_COUNT 3  // 减少缓冲区数量

// ========================== USB传输配置 ==========================

/** @brief 默认TCP服务器端口 */
#define DEFAULT_PORT 8888

/** @brief Luckfox Pico设备默认IP地址 */
#define DEFAULT_SERVER_IP "172.32.0.93"  // Luckfox Pico 默认IP

/** @brief 数据帧头部大小，单位：字节 */
#define HEADER_SIZE 32

/** @brief 网络传输分块大小，64KB以提高网络效率 */
#define CHUNK_SIZE 65536  // 恢复为64KB以提高网络效率

// ========================== 数据结构定义 ==========================

/**
 * @struct buffer
 * @brief 多平面缓冲区结构
 *
 * 用于管理V4L2多平面视频缓冲区，支持最多VIDEO_MAX_PLANES个平面。
 * 每个平面都有独立的内存地址和长度。
 */
struct buffer
{
    void* start[VIDEO_MAX_PLANES];   /**< 每个平面的内存映射起始地址 */
    size_t length[VIDEO_MAX_PLANES]; /**< 每个平面的内存大小 */
    int num_planes;                  /**< 实际使用的平面数量 */
};

/**
 * @struct frame_data
 * @brief 图像帧数据结构
 *
 * 封装单帧图像的所有相关信息，用于线程间数据传递。
 */
struct frame_data
{
    uint8_t* data;      /**< 图像数据指针 */
    size_t size;        /**< 图像数据大小，单位：字节 */
    uint32_t frame_id;  /**< 帧序号，用于跟踪和调试 */
    uint64_t timestamp; /**< 时间戳，单位：纳秒 */
};

// ========================== 全局变量 ==========================

/** @brief 程序运行状态标志，0表示停止，1表示运行 */
volatile int running = 1;

/** @brief 客户端连接状态，0表示未连接，1表示已连接 */
volatile int client_connected = 0;

/** @brief TCP服务器文件描述符 */
int server_fd = -1;

/** @brief 客户端连接文件描述符 */
int client_fd = -1;

/** @brief 帧数据访问互斥锁，保护current_frame的并发访问 */
pthread_mutex_t frame_mutex = PTHREAD_MUTEX_INITIALIZER;

/** @brief 帧准备就绪条件变量，用于线程间同步 */
pthread_cond_t frame_ready = PTHREAD_COND_INITIALIZER;

/** @brief 当前帧数据，在采集线程和发送线程间共享 */
struct frame_data current_frame = {0};

// ========================== 工具函数 ==========================

/**
 * @brief 获取高精度时间戳
 *
 * 使用CLOCK_MONOTONIC时钟获取纳秒级别的时间戳，
 * 该时钟不受系统时间调整影响，适合性能测量。
 *
 * @return 返回纳秒级时间戳
 */
static inline uint64_t get_time_ns()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief 系统信号处理函数
 *
 * 处理SIGINT、SIGTERM等信号，实现程序的优雅退出。
 * 会强制关闭所有阻塞的系统调用，并通知所有等待的线程。
 *
 * @param sig 接收到的信号编号
 */
void signal_handler(int sig)
{
    printf("\nReceived signal %d, shutting down...\n", sig);
    running = 0;

    // 强制关闭服务器socket，打断accept()阻塞
    if (server_fd >= 0)
    {
        shutdown(server_fd, SHUT_RDWR);
    }

    // 关闭客户端连接，打断发送阻塞
    if (client_fd >= 0)
    {
        shutdown(client_fd, SHUT_RDWR);
    }

    // 通知条件变量，唤醒等待的线程
    pthread_mutex_lock(&frame_mutex);
    pthread_cond_broadcast(&frame_ready);
    pthread_mutex_unlock(&frame_mutex);
}

/**
 * @brief V4L2 ioctl系统调用包装函数
 *
 * 提供带重试机制的ioctl调用，自动处理EINTR信号中断。
 * 这是V4L2编程的标准做法，确保系统调用的可靠性。
 *
 * @param fd 文件描述符
 * @param request ioctl请求代码
 * @param arg 请求参数指针
 * @return 成功返回0，失败返回-1
 */
int xioctl(int fd, int request, void* arg)
{
    int r;
    do
    {
        r = ioctl(fd, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}

// ========================== V4L2设备管理函数 ==========================

/**
 * @brief 检查V4L2设备能力
 *
 * 查询摄像头设备的基本信息和支持的功能，确保设备支持：
 * - 多平面视频采集 (V4L2_CAP_VIDEO_CAPTURE_MPLANE)
 * - 流式传输 (V4L2_CAP_STREAMING)
 *
 * @param fd 摄像头设备文件描述符
 * @return 成功返回0，失败返回-1
 */
int check_device_caps(int fd)
{
    struct v4l2_capability cap = {0};

    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == -1)
    {
        perror("VIDIOC_QUERYCAP failed");
        return -1;
    }

    printf("Device: %s\n", cap.card);
    printf("Driver: %s\n", cap.driver);

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE))
    {
        printf("Device does not support multiplanar video capture\n");
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING))
    {
        printf("Device does not support streaming\n");
        return -1;
    }

    printf("Device supports multiplanar streaming capture\n");
    return 0;
}

/**
 * @brief 设置V4L2多平面视频格式
 *
 * 配置摄像头的输出格式，包括分辨率、像素格式等参数。
 * 使用多平面格式以支持更灵活的内存管理。
 *
 * @param fd 摄像头设备文件描述符
 * @param fmt 输出的格式信息结构体指针
 * @return 成功返回0，失败返回-1
 */
int set_format_mp(int fd, struct v4l2_format* fmt)
{
    memset(fmt, 0, sizeof(*fmt));
    fmt->type                   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt->fmt.pix_mp.width       = WIDTH;
    fmt->fmt.pix_mp.height      = HEIGHT;
    fmt->fmt.pix_mp.pixelformat = PIXELFORMAT;
    fmt->fmt.pix_mp.field       = V4L2_FIELD_NONE;

    if (xioctl(fd, VIDIOC_S_FMT, fmt) == -1)
    {
        perror("VIDIOC_S_FMT failed");
        return -1;
    }

    printf("Format set: %dx%d, BG10, %d planes\n",
           fmt->fmt.pix_mp.width,
           fmt->fmt.pix_mp.height,
           fmt->fmt.pix_mp.num_planes);

    return 0;
}

// ========================== V4L2缓冲区管理函数 ==========================

/**
 * @brief 申请并映射V4L2多平面缓冲区
 *
 * 向内核申请指定数量的视频缓冲区，并将它们映射到用户空间。
 * 使用内存映射(mmap)方式，避免数据拷贝，提高性能。
 *
 * @param fd 摄像头设备文件描述符
 * @param buffers 输出的缓冲区数组
 * @param count 请求的缓冲区数量
 * @return 成功返回实际分配的缓冲区数量，失败返回-1
 */
int request_buffers_mp(int fd, struct buffer* buffers, int count)
{
    struct v4l2_requestbuffers reqbuf = {0};
    reqbuf.count                      = count;
    reqbuf.type                       = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbuf.memory                     = V4L2_MEMORY_MMAP;

    if (xioctl(fd, VIDIOC_REQBUFS, &reqbuf) == -1)
    {
        perror("VIDIOC_REQBUFS failed");
        return -1;
    }

    for (int i = 0; i < reqbuf.count; i++)
    {
        struct v4l2_buffer buf                     = {0};
        struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};

        buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory   = V4L2_MEMORY_MMAP;
        buf.index    = i;
        buf.m.planes = planes;
        buf.length   = VIDEO_MAX_PLANES;

        if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1)
        {
            perror("VIDIOC_QUERYBUF failed");
            return -1;
        }

        buffers[i].num_planes = buf.length;

        for (int p = 0; p < buf.length; p++)
        {
            buffers[i].length[p] = buf.m.planes[p].length;
            buffers[i].start[p]  = mmap(NULL,
                                       buf.m.planes[p].length,
                                       PROT_READ | PROT_WRITE,
                                       MAP_SHARED,
                                       fd,
                                       buf.m.planes[p].m.mem_offset);

            if (MAP_FAILED == buffers[i].start[p])
            {
                perror("mmap failed");
                return -1;
            }
        }
    }

    printf("Allocated %d buffers\n", reqbuf.count);
    return reqbuf.count;
}

/**
 * @brief 将缓冲区加入V4L2队列
 *
 * 将空闲的缓冲区提交给驱动程序，用于接收下一帧图像数据。
 *
 * @param fd 摄像头设备文件描述符
 * @param index 缓冲区索引
 * @return 成功返回0，失败返回-1
 */
int queue_buffer_mp(int fd, int index)
{
    struct v4l2_buffer buf                     = {0};
    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};

    buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory   = V4L2_MEMORY_MMAP;
    buf.index    = index;
    buf.m.planes = planes;
    buf.length   = VIDEO_MAX_PLANES;

    return xioctl(fd, VIDIOC_QBUF, &buf);
}

/**
 * @brief 从V4L2队列中取出已填充的缓冲区
 *
 * 从驱动程序队列中取出包含图像数据的缓冲区，获取缓冲区索引和数据大小。
 *
 * @param fd 摄像头设备文件描述符
 * @param index 输出缓冲区索引
 * @param bytes_used 输出实际使用的字节数
 * @return 成功返回0，失败返回-1
 */
int dequeue_buffer_mp(int fd, int* index, size_t* bytes_used)
{
    struct v4l2_buffer buf                     = {0};
    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};

    buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory   = V4L2_MEMORY_MMAP;
    buf.m.planes = planes;
    buf.length   = VIDEO_MAX_PLANES;

    if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1)
    {
        return -1;
    }

    *index      = buf.index;
    *bytes_used = buf.m.planes[0].bytesused;
    return 0;
}

// ========================== V4L2流控制函数 ==========================

/**
 * @brief 启动V4L2视频流
 *
 * 通知驱动程序开始图像采集，此后可以从队列中获取图像数据。
 *
 * @param fd 摄像头设备文件描述符
 * @return 成功返回0，失败返回-1
 */
int start_streaming_mp(int fd)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    if (xioctl(fd, VIDIOC_STREAMON, &type) == -1)
    {
        perror("VIDIOC_STREAMON failed");
        return -1;
    }

    printf("Streaming started\n");
    return 0;
}

/**
 * @brief 停止V4L2视频流
 *
 * 通知驱动程序停止图像采集，清空所有队列中的缓冲区。
 *
 * @param fd 摄像头设备文件描述符
 * @return 成功返回0，失败返回-1
 */
int stop_streaming_mp(int fd)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    if (xioctl(fd, VIDIOC_STREAMOFF, &type) == -1)
    {
        perror("VIDIOC_STREAMOFF failed");
        return -1;
    }

    printf("Streaming stopped\n");
    return 0;
}

// ========================== 网络通信函数 ==========================

/**
 * @brief 创建TCP服务器
 *
 * 创建并配置TCP服务器socket，绑定到指定的IP地址和端口，
 * 开始监听客户端连接请求。
 *
 * @param port 服务器监听端口
 * @return 成功返回服务器socket文件描述符，失败返回-1
 */
int create_server(int port)
{
    int fd;
    struct sockaddr_in addr;
    int opt = 1;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        perror("socket failed");
        return -1;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt failed");
        close(fd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = inet_addr(DEFAULT_SERVER_IP);
    addr.sin_port        = htons(port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        perror("bind failed");
        close(fd);
        return -1;
    }

    if (listen(fd, 1) < 0)
    {
        perror("listen failed");
        close(fd);
        return -1;
    }

    printf("Server listening on %s:%d\n", DEFAULT_SERVER_IP, port);
    return fd;
}

/**
 * @struct frame_header
 * @brief 数据帧头部结构
 *
 * 定义传输协议的帧头格式，包含帧的元数据信息。
 * 使用packed属性确保结构体内存布局的一致性。
 */
struct frame_header
{
    uint32_t magic;       /**< 魔数标识：0xDEADBEEF */
    uint32_t frame_id;    /**< 帧序号 */
    uint32_t width;       /**< 图像宽度 */
    uint32_t height;      /**< 图像高度 */
    uint32_t pixfmt;      /**< 像素格式 */
    uint32_t size;        /**< 数据大小 */
    uint64_t timestamp;   /**< 时间戳 */
    uint32_t reserved[2]; /**< 保留字段 */
} __attribute__((packed));

/**
 * @brief 发送图像帧数据到客户端
 *
 * 将图像数据按照自定义协议发送给TCP客户端。
 * 先发送帧头，再分块发送图像数据，提高传输效率。
 *
 * @param client_fd 客户端socket文件描述符
 * @param data 图像数据指针
 * @param size 图像数据大小
 * @param frame_id 帧序号
 * @param timestamp 时间戳
 * @return 成功返回0，失败返回-1
 */
int send_frame(int client_fd,
               void* data,
               size_t size,
               uint32_t frame_id,
               uint64_t timestamp)
{
    struct frame_header header = {.magic     = 0xDEADBEEF,
                                  .frame_id  = frame_id,
                                  .width     = WIDTH,
                                  .height    = HEIGHT,
                                  .pixfmt    = PIXELFORMAT,
                                  .size      = size,
                                  .timestamp = timestamp,
                                  .reserved  = {0, 0}};

    // 发送帧头
    if (send(client_fd, &header, sizeof(header), MSG_NOSIGNAL) !=
        sizeof(header))
    {
        return -1;
    }

    // 分块发送数据
    size_t sent  = 0;
    uint8_t* ptr = (uint8_t*)data;

    while (sent < size && running)
    {
        size_t to_send =
            (size - sent) > CHUNK_SIZE ? CHUNK_SIZE : (size - sent);
        ssize_t result = send(client_fd, ptr + sent, to_send, MSG_NOSIGNAL);

        if (result <= 0)
        {
            return -1;
        }

        sent += result;
    }

    return 0;
}

// ========================== 多线程处理函数 ==========================

/**
 * @brief USB数据发送线程函数
 *
 * 专门负责网络数据传输的工作线程。主要功能包括：
 * 1. 监听并接受客户端连接
 * 2. 等待采集线程产生的新帧数据
 * 3. 将图像数据发送给已连接的客户端
 * 4. 处理客户端断开连接的情况
 *
 * 该线程与主采集线程通过条件变量和互斥锁进行同步，
 * 确保数据传输的线程安全性。
 *
 * @param arg 线程参数（当前未使用）
 * @return 线程退出时返回NULL
 */
void* usb_sender_thread(void* arg)
{
    printf("USB sender thread started\n");

    while (running)
    {
        // 等待客户端连接
        if (!client_connected)
        {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);

            printf("Waiting for client connection...\n");
            client_fd =
                accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

            if (client_fd < 0)
            {
                if (running)
                {
                    perror("accept failed");
                }
                continue;
            }

            printf("Client connected from %s\n",
                   inet_ntoa(client_addr.sin_addr));
            client_connected = 1;
        }

        // 等待新帧数据
        pthread_mutex_lock(&frame_mutex);
        while (current_frame.data == NULL && running)
        {
            pthread_cond_wait(&frame_ready, &frame_mutex);
        }

        if (current_frame.data && running)
        {
            // 发送帧数据
            if (send_frame(client_fd,
                           current_frame.data,
                           current_frame.size,
                           current_frame.frame_id,
                           current_frame.timestamp) < 0)
            {
                printf("Client disconnected (frame %d)\n",
                       current_frame.frame_id);
                close(client_fd);
                client_connected = 0;

                // 清理当前帧数据，避免内存泄漏
                current_frame.data = NULL;
            }
            else
            {
                // 发送成功，清理当前帧
                current_frame.data = NULL;
            }
        }

        pthread_mutex_unlock(&frame_mutex);
    }

    if (client_connected)
    {
        close(client_fd);
    }

    printf("USB sender thread terminated\n");
    return NULL;
}

// ========================== 图像采集主循环 ==========================

/**
 * @brief 图像采集主循环函数
 *
 * 程序的核心处理循环，负责连续采集摄像头图像数据。主要流程：
 * 1. 使用select()等待摄像头数据就绪
 * 2. 从V4L2队列中取出包含图像的缓冲区
 * 3. 如有客户端连接，通知发送线程处理数据
 * 4. 将缓冲区重新加入队列供下次使用
 * 5. 统计并显示性能信息（帧率、数据量等）
 *
 * 该函数实现了生产者-消费者模式，采集线程作为生产者，
 * 发送线程作为消费者，通过共享内存和同步原语协调工作。
 *
 * @param fd 摄像头设备文件描述符
 * @param buffers 缓冲区数组
 * @param buffer_count 缓冲区数量
 * @return 正常退出返回0
 */
int capture_loop(int fd, struct buffer* buffers, int buffer_count)
{
    uint32_t frame_counter    = 0;
    uint64_t last_stats_time  = get_time_ns();
    uint32_t frames_in_second = 0;

    printf("Starting continuous capture loop...\n");

    while (running)
    {
        // 等待数据可用
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
        int r             = select(fd + 1, &fds, NULL, NULL, &tv);

        if (r == -1)
        {
            if (errno != EINTR)
            {
                perror("select failed");
                break;
            }
            continue;
        }
        else if (r == 0)
        {
            printf("Timeout waiting for frame\n");
            continue;
        }

        // 出队缓冲区
        int buf_index;
        size_t bytes_used;
        if (dequeue_buffer_mp(fd, &buf_index, &bytes_used) < 0)
        {
            if (errno != EAGAIN && errno != EINTR)
            {
                perror("dequeue failed");
                // 尝试恢复而不是直接退出
                sleep(1);
            }
            continue;
        }

        uint64_t timestamp = get_time_ns();

        // 通知USB发送线程（仅在有客户端时）
        if (client_connected)
        {
            pthread_mutex_lock(&frame_mutex);
            current_frame.data      = buffers[buf_index].start[0];
            current_frame.size      = bytes_used;
            current_frame.frame_id  = frame_counter;
            current_frame.timestamp = timestamp;
            pthread_cond_signal(&frame_ready);
            pthread_mutex_unlock(&frame_mutex);
        }

        // 重新队列缓冲区
        if (queue_buffer_mp(fd, buf_index) < 0)
        {
            perror("queue failed");
            // 尝试恢复而不是直接退出
            sleep(1);
            continue;
        }

        frame_counter++;
        frames_in_second++;

        // 统计信息 - 减少输出频率，每5秒输出一次
        uint64_t current_time = get_time_ns();
        if (current_time - last_stats_time >= 5000000000ULL)
        {  // 5秒
            printf("Frame %d, FPS: %d, Bytes: %zu, Connected: %s\n",
                   frame_counter,
                   frames_in_second / 5,
                   bytes_used,
                   client_connected ? "YES" : "NO");
            frames_in_second = 0;
            last_stats_time  = current_time;
        }
    }

    return 0;
}

// ========================== 程序主函数 ==========================

/**
 * @brief 程序入口点
 *
 * 主函数负责整个程序的初始化和协调工作：
 *
 * 1. 参数解析和系统信息检查
 * 2. 信号处理器注册
 * 3. TCP服务器创建和配置
 * 4. V4L2设备初始化（打开、检查能力、设置格式）
 * 5. 内存缓冲区申请和映射
 * 6. 视频流启动
 * 7. 创建网络发送线程
 * 8. 执行图像采集主循环
 * 9. 资源清理和程序退出
 *
 * 程序支持命令行参数指定TCP端口号，默认使用8888端口。
 *
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 程序退出码，成功返回0
 */
int main(int argc, char* argv[])
{
    const char* device = "/dev/video0";
    int port           = DEFAULT_PORT;
    int fd             = -1;
    struct v4l2_format fmt;
    struct buffer buffers[BUFFER_COUNT];
    int buffer_count;
    pthread_t usb_thread;

    if (argc > 1)
    {
        port = atoi(argv[1]);
    }

    printf("V4L2 USB RAW Image Streamer for Luckfox Pico Mini B\n");
    printf("===================================================\n");
    printf("Port: %d\n", port);
    printf("Server IP: %s\n", DEFAULT_SERVER_IP);

    // 检查系统资源
    printf("Checking system resources...\n");
    system(
        "free -m | head -2 | tail -1 | awk '{print \"Memory: \" $3 \"/\" $2 \" "
        "MB used\"}'");

    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);  // 忽略SIGPIPE信号

    // 创建服务器
    server_fd = create_server(port);
    if (server_fd < 0)
    {
        return -1;
    }

    // 打开摄像头设备
    fd = open(device, O_RDWR | O_NONBLOCK);
    if (fd == -1)
    {
        perror("Failed to open camera device");
        close(server_fd);
        return -1;
    }

    printf("Opened camera: %s\n", device);

    // 检查设备能力
    if (check_device_caps(fd) < 0)
    {
        goto cleanup;
    }

    // 设置格式
    if (set_format_mp(fd, &fmt) < 0)
    {
        goto cleanup;
    }

    // 申请缓冲区
    buffer_count = request_buffers_mp(fd, buffers, BUFFER_COUNT);
    if (buffer_count < 0)
    {
        goto cleanup;
    }

    // 队列所有缓冲区
    for (int i = 0; i < buffer_count; i++)
    {
        if (queue_buffer_mp(fd, i) < 0)
        {
            goto cleanup;
        }
    }

    // 启动流
    if (start_streaming_mp(fd) < 0)
    {
        goto cleanup;
    }

    // 启动USB发送线程
    if (pthread_create(&usb_thread, NULL, usb_sender_thread, NULL) != 0)
    {
        perror("Failed to create USB thread");
        goto cleanup;
    }

    // 主采集循环
    capture_loop(fd, buffers, buffer_count);

    // 等待线程结束
    pthread_join(usb_thread, NULL);

cleanup:
    if (fd >= 0)
    {
        stop_streaming_mp(fd);

        // 清理映射
        for (int i = 0; i < buffer_count; i++)
        {
            for (int p = 0; p < buffers[i].num_planes; p++)
            {
                if (buffers[i].start[p] != MAP_FAILED)
                {
                    munmap(buffers[i].start[p], buffers[i].length[p]);
                }
            }
        }

        close(fd);
    }

    if (server_fd >= 0)
    {
        close(server_fd);
    }

    printf("Program terminated\n");
    return 0;
}
