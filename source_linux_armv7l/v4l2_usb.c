#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/videodev2.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>
#include <signal.h>
#include <pthread.h>

#define WIDTH 2048
#define HEIGHT 1296
#define PIXELFORMAT V4L2_PIX_FMT_SBGGR10
#define BUFFER_COUNT 3  // 减少缓冲区数量

// USB传输配置
#define DEFAULT_PORT 8888
#define DEFAULT_SERVER_IP "172.32.0.93"  // Luckfox Pico 默认IP
#define HEADER_SIZE 32
#define CHUNK_SIZE 65536  // 恢复为64KB以提高网络效率

// 多平面缓冲区结构
struct buffer {
    void *start[VIDEO_MAX_PLANES];
    size_t length[VIDEO_MAX_PLANES];
    int num_planes;
};

// 图像数据结构
struct frame_data {
    uint8_t *data;
    size_t size;
    uint32_t frame_id;
    uint64_t timestamp;
};

// 全局状态
volatile int running = 1;
volatile int client_connected = 0;
int server_fd = -1;
int client_fd = -1;
pthread_mutex_t frame_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t frame_ready = PTHREAD_COND_INITIALIZER;
struct frame_data current_frame = {0};

// 高精度计时函数
static inline uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// 信号处理 - 增强版，强制关闭所有阻塞调用
void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    running = 0;
    
    // 强制关闭服务器socket，打断accept()阻塞
    if (server_fd >= 0) {
        shutdown(server_fd, SHUT_RDWR);
    }
    
    // 关闭客户端连接，打断发送阻塞
    if (client_fd >= 0) {
        shutdown(client_fd, SHUT_RDWR);
    }
    
    // 通知条件变量，唤醒等待的线程
    pthread_mutex_lock(&frame_mutex);
    pthread_cond_broadcast(&frame_ready);
    pthread_mutex_unlock(&frame_mutex);
}

int xioctl(int fd, int request, void *arg) {
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}

// 检查设备能力
int check_device_caps(int fd) {
    struct v4l2_capability cap = {0};
    
    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        perror("VIDIOC_QUERYCAP failed");
        return -1;
    }
    
    printf("Device: %s\n", cap.card);
    printf("Driver: %s\n", cap.driver);
    
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
        printf("Device does not support multiplanar video capture\n");
        return -1;
    }
    
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        printf("Device does not support streaming\n");
        return -1;
    }
    
    printf("Device supports multiplanar streaming capture\n");
    return 0;
}

// 设置多平面格式
int set_format_mp(int fd, struct v4l2_format *fmt) {
    memset(fmt, 0, sizeof(*fmt));
    fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt->fmt.pix_mp.width = WIDTH;
    fmt->fmt.pix_mp.height = HEIGHT;
    fmt->fmt.pix_mp.pixelformat = PIXELFORMAT;
    fmt->fmt.pix_mp.field = V4L2_FIELD_NONE;
    
    if (xioctl(fd, VIDIOC_S_FMT, fmt) == -1) {
        perror("VIDIOC_S_FMT failed");
        return -1;
    }
    
    printf("Format set: %dx%d, BG10, %d planes\n",
           fmt->fmt.pix_mp.width, fmt->fmt.pix_mp.height,
           fmt->fmt.pix_mp.num_planes);
    
    return 0;
}

// 申请多平面缓冲区
int request_buffers_mp(int fd, struct buffer *buffers, int count) {
    struct v4l2_requestbuffers reqbuf = {0};
    reqbuf.count = count;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    
    if (xioctl(fd, VIDIOC_REQBUFS, &reqbuf) == -1) {
        perror("VIDIOC_REQBUFS failed");
        return -1;
    }
    
    for (int i = 0; i < reqbuf.count; i++) {
        struct v4l2_buffer buf = {0};
        struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};
        
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.m.planes = planes;
        buf.length = VIDEO_MAX_PLANES;
        
        if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            perror("VIDIOC_QUERYBUF failed");
            return -1;
        }
        
        buffers[i].num_planes = buf.length;
        
        for (int p = 0; p < buf.length; p++) {
            buffers[i].length[p] = buf.m.planes[p].length;
            buffers[i].start[p] = mmap(NULL, buf.m.planes[p].length,
                                      PROT_READ | PROT_WRITE, MAP_SHARED,
                                      fd, buf.m.planes[p].m.mem_offset);
            
            if (MAP_FAILED == buffers[i].start[p]) {
                perror("mmap failed");
                return -1;
            }
        }
    }
    
    printf("Allocated %d buffers\n", reqbuf.count);
    return reqbuf.count;
}

// 队列/出队缓冲区
int queue_buffer_mp(int fd, int index) {
    struct v4l2_buffer buf = {0};
    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;
    buf.m.planes = planes;
    buf.length = VIDEO_MAX_PLANES;
    
    return xioctl(fd, VIDIOC_QBUF, &buf);
}

int dequeue_buffer_mp(int fd, int *index, size_t *bytes_used) {
    struct v4l2_buffer buf = {0};
    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.m.planes = planes;
    buf.length = VIDEO_MAX_PLANES;
    
    if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
        return -1;
    }
    
    *index = buf.index;
    *bytes_used = buf.m.planes[0].bytesused;
    return 0;
}

// 启动/停止流
int start_streaming_mp(int fd) {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    
    if (xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        perror("VIDIOC_STREAMON failed");
        return -1;
    }
    
    printf("Streaming started\n");
    return 0;
}

int stop_streaming_mp(int fd) {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    
    if (xioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
        perror("VIDIOC_STREAMOFF failed");
        return -1;
    }
    
    printf("Streaming stopped\n");
    return 0;
}

// 创建TCP服务器
int create_server(int port) {
    int fd;
    struct sockaddr_in addr;
    int opt = 1;
    
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket failed");
        return -1;
    }
    
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(fd);
        return -1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(DEFAULT_SERVER_IP);
    addr.sin_port = htons(port);
    
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        close(fd);
        return -1;
    }
    
    if (listen(fd, 1) < 0) {
        perror("listen failed");
        close(fd);
        return -1;
    }
    
    printf("Server listening on %s:%d\n", DEFAULT_SERVER_IP, port);
    return fd;
}

// 发送数据帧头
struct frame_header {
    uint32_t magic;      // 0xDEADBEEF
    uint32_t frame_id;
    uint32_t width;
    uint32_t height;
    uint32_t pixfmt;
    uint32_t size;
    uint64_t timestamp;
    uint32_t reserved[2];
} __attribute__((packed));

// 发送帧数据
int send_frame(int client_fd, void *data, size_t size, uint32_t frame_id, uint64_t timestamp) {
    struct frame_header header = {
        .magic = 0xDEADBEEF,
        .frame_id = frame_id,
        .width = WIDTH,
        .height = HEIGHT,
        .pixfmt = PIXELFORMAT,
        .size = size,
        .timestamp = timestamp,
        .reserved = {0, 0}
    };
    
    // 发送帧头
    if (send(client_fd, &header, sizeof(header), MSG_NOSIGNAL) != sizeof(header)) {
        return -1;
    }
    
    // 分块发送数据
    size_t sent = 0;
    uint8_t *ptr = (uint8_t*)data;
    
    while (sent < size && running) {
        size_t to_send = (size - sent) > CHUNK_SIZE ? CHUNK_SIZE : (size - sent);
        ssize_t result = send(client_fd, ptr + sent, to_send, MSG_NOSIGNAL);
        
        if (result <= 0) {
            return -1;
        }
        
        sent += result;
    }
    
    return 0;
}

// USB传输线程
void* usb_sender_thread(void* arg) {
    printf("USB sender thread started\n");
    
    while (running) {
        // 等待客户端连接
        if (!client_connected) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            printf("Waiting for client connection...\n");
            client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_fd < 0) {
                if (running) {
                    perror("accept failed");
                }
                continue;
            }
            
            printf("Client connected from %s\n", inet_ntoa(client_addr.sin_addr));
            client_connected = 1;
        }
        
        // 等待新帧数据
        pthread_mutex_lock(&frame_mutex);
        while (current_frame.data == NULL && running) {
            pthread_cond_wait(&frame_ready, &frame_mutex);
        }
        
        if (current_frame.data && running) {
            // 发送帧数据
            if (send_frame(client_fd, current_frame.data, current_frame.size, 
                          current_frame.frame_id, current_frame.timestamp) < 0) {
                printf("Client disconnected (frame %d)\n", current_frame.frame_id);
                close(client_fd);
                client_connected = 0;
                
                // 清理当前帧数据，避免内存泄漏
                current_frame.data = NULL;
            } else {
                // 发送成功，清理当前帧
                current_frame.data = NULL;
            }
        }
        
        pthread_mutex_unlock(&frame_mutex);
    }
    
    if (client_connected) {
        close(client_fd);
    }
    
    printf("USB sender thread terminated\n");
    return NULL;
}

// 图像采集主循环
int capture_loop(int fd, struct buffer *buffers, int buffer_count) {
    uint32_t frame_counter = 0;
    uint64_t last_stats_time = get_time_ns();
    uint32_t frames_in_second = 0;
    
    printf("Starting continuous capture loop...\n");
    
    while (running) {
        // 等待数据可用
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        
        struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
        int r = select(fd + 1, &fds, NULL, NULL, &tv);
        
        if (r == -1) {
            if (errno != EINTR) {
                perror("select failed");
                break;
            }
            continue;
        } else if (r == 0) {
            printf("Timeout waiting for frame\n");
            continue;
        }
        
        // 出队缓冲区
        int buf_index;
        size_t bytes_used;
        if (dequeue_buffer_mp(fd, &buf_index, &bytes_used) < 0) {
            if (errno != EAGAIN && errno != EINTR) {
                perror("dequeue failed");
                // 尝试恢复而不是直接退出
                sleep(1);
            }
            continue;
        }
        
        uint64_t timestamp = get_time_ns();
        
        // 通知USB发送线程（仅在有客户端时）
        if (client_connected) {
            pthread_mutex_lock(&frame_mutex);
            current_frame.data = buffers[buf_index].start[0];
            current_frame.size = bytes_used;
            current_frame.frame_id = frame_counter;
            current_frame.timestamp = timestamp;
            pthread_cond_signal(&frame_ready);
            pthread_mutex_unlock(&frame_mutex);
        }
        
        // 重新队列缓冲区
        if (queue_buffer_mp(fd, buf_index) < 0) {
            perror("queue failed");
            // 尝试恢复而不是直接退出
            sleep(1);
            continue;
        }
        
        frame_counter++;
        frames_in_second++;
        
        // 统计信息 - 减少输出频率，每5秒输出一次
        uint64_t current_time = get_time_ns();
        if (current_time - last_stats_time >= 5000000000ULL) {  // 5秒            
            printf("Frame %d, FPS: %d, Bytes: %zu, Connected: %s\n", 
                   frame_counter, frames_in_second/5, bytes_used, 
                   client_connected ? "YES" : "NO");
            frames_in_second = 0;
            last_stats_time = current_time;
        }
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    const char *device = "/dev/video0";
    int port = DEFAULT_PORT;
    int fd = -1;
    struct v4l2_format fmt;
    struct buffer buffers[BUFFER_COUNT];
    int buffer_count;
    pthread_t usb_thread;
    
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    printf("V4L2 USB RAW Image Streamer for Luckfox Pico Mini B\n");
    printf("===================================================\n");
    printf("Port: %d\n", port);
    printf("Server IP: %s\n", DEFAULT_SERVER_IP);
    
    // 检查系统资源
    printf("Checking system resources...\n");
    system("free -m | head -2 | tail -1 | awk '{print \"Memory: \" $3 \"/\" $2 \" MB used\"}'");
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);  // 忽略SIGPIPE信号
    
    // 创建服务器
    server_fd = create_server(port);
    if (server_fd < 0) {
        return -1;
    }
    
    // 打开摄像头设备
    fd = open(device, O_RDWR | O_NONBLOCK);
    if (fd == -1) {
        perror("Failed to open camera device");
        close(server_fd);
        return -1;
    }
    
    printf("Opened camera: %s\n", device);
    
    // 检查设备能力
    if (check_device_caps(fd) < 0) {
        goto cleanup;
    }
    
    // 设置格式
    if (set_format_mp(fd, &fmt) < 0) {
        goto cleanup;
    }
    
    // 申请缓冲区
    buffer_count = request_buffers_mp(fd, buffers, BUFFER_COUNT);
    if (buffer_count < 0) {
        goto cleanup;
    }
    
    // 队列所有缓冲区
    for (int i = 0; i < buffer_count; i++) {
        if (queue_buffer_mp(fd, i) < 0) {
            goto cleanup;
        }
    }
    
    // 启动流
    if (start_streaming_mp(fd) < 0) {
        goto cleanup;
    }
    
    // 启动USB发送线程
    if (pthread_create(&usb_thread, NULL, usb_sender_thread, NULL) != 0) {
        perror("Failed to create USB thread");
        goto cleanup;
    }
    
    // 主采集循环
    capture_loop(fd, buffers, buffer_count);
    
    // 等待线程结束
    pthread_join(usb_thread, NULL);
    
cleanup:
    if (fd >= 0) {
        stop_streaming_mp(fd);
        
        // 清理映射
        for (int i = 0; i < buffer_count; i++) {
            for (int p = 0; p < buffers[i].num_planes; p++) {
                if (buffers[i].start[p] != MAP_FAILED) {
                    munmap(buffers[i].start[p], buffers[i].length[p]);
                }
            }
        }
        
        close(fd);
    }
    
    if (server_fd >= 0) {
        close(server_fd);
    }
    
    printf("Program terminated\n");
    return 0;
}
