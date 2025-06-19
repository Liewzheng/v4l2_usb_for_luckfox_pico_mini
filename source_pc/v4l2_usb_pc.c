#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/time.h>

#define _POSIX_C_SOURCE 199309L

// 默认配置
#define DEFAULT_SERVER_IP "192.168.230.93"
#define DEFAULT_PORT 8888
#define OUTPUT_DIR "./received_frames"
#define MAX_FILENAME_LEN 256
#define RECV_TIMEOUT_SEC 10

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
    uint32_t bytes_received;
    uint64_t start_time;
    uint64_t last_frame_time;
    double avg_fps;
} stats = {0};

static inline uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    running = 0;
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
            } else {
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
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket failed");
        return -1;
    }
    
    // 设置接收超时
    timeout.tv_sec = RECV_TIMEOUT_SEC;
    timeout.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt failed");
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
    
    snprintf(filename, sizeof(filename), "%s/frame_%06d_%dx%d.%s", 
             OUTPUT_DIR, frame_id, width, height, ext);
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("Failed to open output file");
        return -1;
    }
    
    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);
    
    if (written != size) {
        printf("Warning: wrote %zu bytes instead of %zu\n", written, size);
        return -1;
    }
    
    return 0;
}

// 打印帧信息
void print_frame_info(const struct frame_header *header) {
    printf("Frame %d: %dx%d, pixfmt=0x%08x (%c%c%c%c), size=%d bytes, ",
           header->frame_id, header->width, header->height,
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

// 更新统计信息
void update_stats(uint32_t frame_size) {
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

// 打印统计信息
void print_stats() {
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

// 主接收循环
int receive_loop(int sock) {
    uint8_t *frame_buffer = NULL;
    size_t buffer_size = 0;
    int save_enabled = 1;
    int save_interval = 10;  // 每10帧保存一次
    
    printf("Starting receive loop (Ctrl+C to stop)...\n");
    printf("Frames will be saved to: %s\n", OUTPUT_DIR);
    
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
        if (header.size == 0 || header.size > 50 * 1024 * 1024) {  // 最大50MB
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
            printf("Allocated %zu bytes frame buffer\n", buffer_size);
        }
        
        // 接收帧数据
        if (recv_full(sock, frame_buffer, header.size) < 0) {
            break;
        }
        
        // 打印帧信息
        print_frame_info(&header);
        
        // 保存帧（根据设置）
        if (save_enabled && (header.frame_id % save_interval == 0)) {
            if (save_frame(frame_buffer, header.size, header.frame_id,
                          header.width, header.height, header.pixfmt) == 0) {
                printf("  -> Saved to file\n");
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

void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("Options:\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -s, --server IP     Server IP address (default: %s)\n", DEFAULT_SERVER_IP);
    printf("  -p, --port PORT     Server port (default: %d)\n", DEFAULT_PORT);
    printf("  -o, --output DIR    Output directory (default: %s)\n", OUTPUT_DIR);
    printf("\nExample:\n");
    printf("  %s -s 192.168.1.100 -p 8888 -o ./frames\n", prog_name);
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
    
    printf("V4L2 USB RAW Image Receiver (PC Client)\n");
    printf("=======================================\n");
    printf("Server: %s:%d\n", server_ip, port);
    printf("Output: %s\n", output_dir);
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
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
    
    // 清理
    close(sock_fd);
    
    // 打印最终统计
    print_stats();
    
    printf("Program terminated\n");
    return result;
}
