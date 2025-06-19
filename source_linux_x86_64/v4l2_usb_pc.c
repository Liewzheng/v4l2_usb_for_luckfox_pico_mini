#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/time.h>

// 默认配置
#define DEFAULT_SERVER_IP "172.32.0.93"  // Luckfox Pico 默认IP
#define DEFAULT_PORT 8888
#define OUTPUT_DIR "./received_frames"
#define MAX_FILENAME_LEN 256
#define RECV_TIMEOUT_SEC 10
#define RECV_BUFFER_SIZE (8 * 1024 * 1024)  // 8MB接收缓冲区

// 帧头结构（与设备端保持一致）
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

// 全局状态
volatile int running = 1;
int sock_fd = -1;

// 统计信息
struct stats {
    uint32_t frames_received;
    uint64_t bytes_received;  // 改为64位防止溢出
    uint64_t start_time;
    uint64_t last_frame_time;
    double avg_fps;
    double avg_mbps;  // 新增平均传输速度
} stats = {0};

static inline uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down gracefully...\n", sig);
    running = 0;
    if (sock_fd >= 0) {
        close(sock_fd);
    }
}

// 创建输出目录
int create_output_dir(const char *dir) {
    struct stat st = {0};
    
    if (stat(dir, &st) == -1) {
        if (mkdir(dir, 0755) == -1) {
            perror("Failed to create output directory");
            return -1;
        }
        printf("Created output directory: %s\n", dir);
    }
    
    return 0;
}

// 可靠地接收指定字节数的数据
int recv_full(int sock, void *buffer, size_t size) {
    size_t received = 0;
    uint8_t *ptr = (uint8_t*)buffer;
    
    while (received < size && running) {
        ssize_t result = recv(sock, ptr + received, size - received, 0);
        
        if (result <= 0) {
            if (result == 0) {
                printf("Connection closed by server\n");
            } else if (errno != EINTR) {
                perror("recv failed");
            }
            return -1;
        }
        
        received += result;
    }
    
    return received == size ? 0 : -1;
}

// 连接到服务器
int connect_to_server(const char *ip, int port) {
    int sock;
    struct sockaddr_in server_addr;
    struct timeval timeout;
    int opt = 1;
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket failed");
        return -1;
    }
    
    // 设置socket选项以提高性能
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
    }
    
    // 设置接收缓冲区大小
    int recv_buf_size = RECV_BUFFER_SIZE;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &recv_buf_size, sizeof(recv_buf_size)) < 0) {
        perror("setsockopt SO_RCVBUF failed");
    }
    
    // 设置发送缓冲区大小
    int send_buf_size = RECV_BUFFER_SIZE;
    if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &send_buf_size, sizeof(send_buf_size)) < 0) {
        perror("setsockopt SO_SNDBUF failed");
    }
    
    // 禁用Nagle算法以减少延迟
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
        perror("setsockopt TCP_NODELAY failed");
    }
    
    // 设置接收超时
    timeout.tv_sec = RECV_TIMEOUT_SEC;
    timeout.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt SO_RCVTIMEO failed");
        close(sock);
        return -1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        printf("Invalid server IP address: %s\n", ip);
        close(sock);
        return -1;
    }
    
    printf("Connecting to %s:%d...\n", ip, port);
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        close(sock);
        return -1;
    }
    
    printf("Connected successfully!\n");
    printf("Socket optimizations:\n");
    printf("  • Receive buffer: %d MB\n", recv_buf_size / (1024*1024));
    printf("  • Send buffer: %d MB\n", send_buf_size / (1024*1024));
    printf("  • TCP_NODELAY: enabled\n");
    return sock;
}

// 保存帧数据到文件
int save_frame(const uint8_t *data, size_t size, uint32_t frame_id, 
               uint32_t width, uint32_t height, uint32_t pixfmt) {
    char filename[MAX_FILENAME_LEN];
    
    // 根据像素格式确定文件扩展名
    const char *ext = "raw";
    if (pixfmt == 0x30314742) {  // V4L2_PIX_FMT_SBGGR10
        ext = "BG10";
    }
    
    snprintf(filename, sizeof(filename), "%s/frame_%06u_%ux%u.%s", 
             OUTPUT_DIR, frame_id, width, height, ext);
    
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open output file");
        return -1;
    }
    
    size_t written = fwrite(data, 1, size, file);
    fclose(file);
    
    if (written != size) {
        printf("Warning: only wrote %zu of %zu bytes\n", written, size);
        return -1;
    }
    
    return 0;
}

// 打印帧信息（简化版）
void print_frame_info(const struct frame_header *header) {
    // 只在需要时打印详细信息
    if (header->frame_id % 100 == 0) {
        printf("Frame %u: %ux%u, format=0x%08x, size=%u bytes\n",
               header->frame_id, header->width, header->height,
               header->pixfmt, header->size);
    }
}

// 更新统计信息
void update_stats(uint32_t frame_size) {
    uint64_t current_time = get_time_ns();
    
    if (stats.start_time == 0) {
        stats.start_time = current_time;
    }
    
    stats.frames_received++;
    stats.bytes_received += frame_size;
    
    // 计算FPS和传输速度
    uint64_t elapsed = current_time - stats.start_time;
    if (elapsed > 0) {
        stats.avg_fps = (double)stats.frames_received * 1000000000.0 / elapsed;
        stats.avg_mbps = ((double)stats.bytes_received / 1024.0 / 1024.0) * 1000000000.0 / elapsed;
    }
    
    stats.last_frame_time = current_time;
}

// 打印统计信息
void print_stats() {
    uint64_t current_time = get_time_ns();
    double elapsed_sec = (current_time - stats.start_time) / 1000000000.0;
    double mbps = ((double)stats.bytes_received / 1024.0 / 1024.0) / elapsed_sec;
    
    printf("\n=== Performance Statistics ===\n");
    printf("Frames received: %u\n", stats.frames_received);
    printf("Bytes received: %llu (%.2f MB)\n", (unsigned long long)stats.bytes_received, 
           (double)stats.bytes_received / 1024.0 / 1024.0);
    printf("Elapsed time: %.2f seconds\n", elapsed_sec);
    printf("Average FPS: %.2f\n", stats.avg_fps);
    printf("Data rate: %.2f MB/s\n", mbps);
    printf("Network efficiency: %.1f%%\n", 
           stats.avg_fps > 0 ? (stats.avg_fps / 30.0) * 100.0 : 0.0);  // 假设目标30FPS
}

// 主接收循环
int receive_loop(int sock) {
    uint8_t *frame_buffer = NULL;
    size_t buffer_size = 0;
    int save_enabled = 1;
    int save_interval = 30;  // 每30帧保存一次，减少I/O负载
    uint64_t last_stats_time = 0;
    
    printf("Starting receive loop (Ctrl+C to stop)...\n");
    printf("Frames will be saved to: %s (every %d frames)\n", OUTPUT_DIR, save_interval);
    printf("Optimizations: Large buffers, TCP_NODELAY, reduced file I/O\n");
    
    while (running) {
        struct frame_header header;
        
        // 接收帧头
        if (recv_full(sock, &header, sizeof(header)) < 0) {
            if (running) {
                printf("Failed to receive frame header\n");
            }
            break;
        }
        
        // 验证魔数
        if (header.magic != 0xDEADBEEF) {
            printf("Invalid frame magic: 0x%08x (expected: 0xDEADBEEF)\n", header.magic);
            break;
        }
        
        // 检查帧大小合理性
        if (header.size == 0 || header.size > 50 * 1024 * 1024) {  // 最大50MB
            printf("Invalid frame size: %u bytes\n", header.size);
            break;
        }
        
        // 动态分配缓冲区（如果需要）
        if (header.size > buffer_size) {
            uint8_t *new_buffer = realloc(frame_buffer, header.size);
            if (!new_buffer) {
                printf("Failed to allocate %u bytes for frame buffer\n", header.size);
                break;
            }
            frame_buffer = new_buffer;
            buffer_size = header.size;
            printf("Reallocated frame buffer to %zu bytes\n", buffer_size);
        }
        
        // 接收帧数据
        if (recv_full(sock, frame_buffer, header.size) < 0) {
            if (running) {
                printf("Failed to receive frame data\n");
            }
            break;
        }
        
        // 更新统计
        update_stats(header.size);
        
        // 每30帧打印一次简化的帧信息
        if (stats.frames_received % 30 == 0) {
            printf("Frame %u: %ux%u, %u bytes, FPS: %.1f, Rate: %.1f MB/s\n", 
                   header.frame_id, header.width, header.height, header.size,
                   stats.avg_fps, stats.avg_mbps);
        }
        
        // 保存帧（根据设置，减少I/O负载）
        if (save_enabled && (header.frame_id % save_interval == 0)) {
            if (save_frame(frame_buffer, header.size, header.frame_id,
                          header.width, header.height, header.pixfmt) == 0) {
                printf("  -> Saved frame %u to file\n", header.frame_id);
            }
        }
        
        // 每5秒显示一次详细统计
        uint64_t current_time = get_time_ns();
        if (current_time - last_stats_time >= 5000000000ULL) {  // 5秒
            printf("\n=== Real-time Stats ===\n");
            printf("Frames: %u, FPS: %.2f, Rate: %.2f MB/s, Efficiency: %.1f%%\n",
                   stats.frames_received, stats.avg_fps, stats.avg_mbps,
                   stats.avg_fps > 0 ? (stats.avg_fps / 30.0) * 100.0 : 0.0);
            last_stats_time = current_time;
        }
    }
    
    free(frame_buffer);
    return 0;
}

void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("V4L2 USB RAW Image Receiver - High Performance PC Client\n\n");
    printf("Options:\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -s, --server IP     Server IP address (default: %s)\n", DEFAULT_SERVER_IP);
    printf("  -p, --port PORT     Server port (default: %d)\n", DEFAULT_PORT);
    printf("  -o, --output DIR    Output directory (default: %s)\n", OUTPUT_DIR);
    printf("\nFeatures:\n");
    printf("  • Large socket buffers (%d MB) for better throughput\n", RECV_BUFFER_SIZE / (1024*1024));
    printf("  • TCP_NODELAY enabled for reduced latency\n");
    printf("  • Intelligent frame saving (every 30 frames)\n");
    printf("  • Real-time performance monitoring\n");
    printf("  • Optimized for 2048x1296 RAW10 streams\n");
    printf("\nExample:\n");
    printf("  %s -s 172.32.0.93 -p 8888 -o ./frames\n", prog_name);
    printf("\nExpected Performance:\n");
    printf("  • Target FPS: 30\n");
    printf("  • Data rate: ~160 MB/s for RAW10 format\n");
    printf("  • Network efficiency should be >90%%\n");
}

int main(int argc, char *argv[]) {
    const char *server_ip = DEFAULT_SERVER_IP;
    int port = DEFAULT_PORT;
    const char *output_dir = OUTPUT_DIR;
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--server") == 0) {
            if (++i < argc) {
                server_ip = argv[i];
            } else {
                printf("Error: --server requires an IP address\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (++i < argc) {
                port = atoi(argv[i]);
                if (port <= 0 || port > 65535) {
                    printf("Error: Invalid port number\n");
                    return 1;
                }
            } else {
                printf("Error: --port requires a port number\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (++i < argc) {
                output_dir = argv[i];
            } else {
                printf("Error: --output requires a directory path\n");
                return 1;
            }
        } else {
            printf("Error: Unknown option %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    printf("V4L2 USB RAW Image Receiver (PC Client) - High Performance Edition\n");
    printf("==================================================================\n");
    printf("Server: %s:%d\n", server_ip, port);
    printf("Output: %s\n", output_dir);
    printf("Buffer size: %d MB\n", RECV_BUFFER_SIZE / (1024*1024));
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);  // 忽略SIGPIPE信号
    
    // 创建输出目录
    if (create_output_dir(output_dir) < 0) {
        return 1;
    }
    
    // 连接到服务器
    sock_fd = connect_to_server(server_ip, port);
    if (sock_fd < 0) {
        return 1;
    }
    
    // 主接收循环
    int result = receive_loop(sock_fd);
    
    // 清理资源
    if (sock_fd >= 0) {
        close(sock_fd);
    }
    
    // 显示最终统计
    print_stats();
    
    printf("\nClient terminated%s\n", result == 0 ? " normally" : " with errors");
    return result;
}
