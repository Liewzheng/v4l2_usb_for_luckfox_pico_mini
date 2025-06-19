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
#include <linux/videodev2.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>

#define WIDTH 2048
#define HEIGHT 1296
#define PIXELFORMAT V4L2_PIX_FMT_SBGGR10
#define BUFFER_COUNT 3
#define FILENAME_TEMPLATE "/dev/shm/raw_%02d.BG10"
#define WARMUP_RUNS 5
#define MAX_RUNS 100  // 减少运行次数以避免频繁写入
#define SAVE_INTERVAL 10  // 每10帧保存一次，减少磁盘写入

#ifndef uint64_t
typedef unsigned long long uint64_t;
#endif

#ifndef UINT64_MAX
#define UINT64_MAX ((uint64_t)-1)
#endif

// 高精度计时函数
static inline uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

int xioctl(int fd, int request, void *arg) {
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}

void print_format_info(struct v4l2_format *fmt) {
    printf("Format: %dx%d, pixelformat=0x%08x (%c%c%c%c), bytesperline=%d, sizeimage=%d\n",
           fmt->fmt.pix.width, fmt->fmt.pix.height,
           fmt->fmt.pix.pixelformat,
           (fmt->fmt.pix.pixelformat >> 0) & 0xFF,
           (fmt->fmt.pix.pixelformat >> 8) & 0xFF,
           (fmt->fmt.pix.pixelformat >> 16) & 0xFF,
           (fmt->fmt.pix.pixelformat >> 24) & 0xFF,
           fmt->fmt.pix.bytesperline,
           fmt->fmt.pix.sizeimage);
}

void enumerate_formats(int fd) {
    struct v4l2_fmtdesc fmtdesc = {0};
    struct v4l2_frmsizeenum frmsizeenum = {0};
    
    printf("Available formats:\n");
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    for (fmtdesc.index = 0; ; fmtdesc.index++) {
        if (xioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == -1) {
            if (fmtdesc.index == 0) {
                printf("  No formats available or enumeration failed\n");
            }
            break;
        }
        
        printf("  %d: %s (0x%08x - %c%c%c%c)\n", 
               fmtdesc.index, 
               fmtdesc.description,
               fmtdesc.pixelformat,
               (fmtdesc.pixelformat >> 0) & 0xFF,
               (fmtdesc.pixelformat >> 8) & 0xFF,
               (fmtdesc.pixelformat >> 16) & 0xFF,
               (fmtdesc.pixelformat >> 24) & 0xFF);
        
        // 枚举该格式的帧大小
        frmsizeenum.pixel_format = fmtdesc.pixelformat;
        for (frmsizeenum.index = 0; ; frmsizeenum.index++) {
            if (xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum) == -1) {
                break;
            }
            
            if (frmsizeenum.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                printf("    %dx%d\n", 
                       frmsizeenum.discrete.width, 
                       frmsizeenum.discrete.height);
            } else if (frmsizeenum.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
                printf("    %dx%d to %dx%d (step %dx%d)\n",
                       frmsizeenum.stepwise.min_width,
                       frmsizeenum.stepwise.min_height,
                       frmsizeenum.stepwise.max_width,
                       frmsizeenum.stepwise.max_height,
                       frmsizeenum.stepwise.step_width,
                       frmsizeenum.stepwise.step_height);
            }
        }
    }
}

// 尝试多种常见格式
int try_common_formats(int fd, struct v4l2_format *fmt) {
    // 常见的格式列表
    struct {
        uint32_t pixelformat;
        const char* name;
        int width, height;
    } formats[] = {
        {V4L2_PIX_FMT_SBGGR10, "SBGGR10", 2048, 1296},
        {V4L2_PIX_FMT_SBGGR10, "SBGGR10", 1920, 1080},
        {V4L2_PIX_FMT_SBGGR8, "SBGGR8", 2048, 1296},
        {V4L2_PIX_FMT_SBGGR8, "SBGGR8", 1920, 1080},
        {V4L2_PIX_FMT_YUYV, "YUYV", 1920, 1080},
        {V4L2_PIX_FMT_YUYV, "YUYV", 1280, 720},
        {V4L2_PIX_FMT_NV12, "NV12", 1920, 1080},
        {V4L2_PIX_FMT_NV16, "NV16", 1920, 1080},
        {0, NULL, 0, 0}
    };
    
    printf("Trying common formats...\n");
    
    for (int i = 0; formats[i].pixelformat != 0; i++) {
        fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt->fmt.pix.width = formats[i].width;
        fmt->fmt.pix.height = formats[i].height;
        fmt->fmt.pix.pixelformat = formats[i].pixelformat;
        fmt->fmt.pix.field = V4L2_FIELD_NONE;
        
        printf("  Trying %s %dx%d...", formats[i].name, formats[i].width, formats[i].height);
        
        if (xioctl(fd, VIDIOC_S_FMT, fmt) == 0) {
            printf(" SUCCESS!\n");
            return 0;
        } else {
            printf(" failed\n");
        }
    }
    
    return -1;
}

typedef struct {
    void* start;
    size_t length;
} Buffer;

// 单帧捕获函数（使用read方式）
int capture_single_frame(int fd, const char *filename) {
    // 分配缓冲区
    size_t buffer_size = WIDTH * HEIGHT * 10 / 8;  // 10位数据
    unsigned char *buffer = malloc(buffer_size);
    if (!buffer) {
        perror("Failed to allocate buffer");
        return -1;
    }
    
    // 读取一帧数据
    ssize_t bytes_read = read(fd, buffer, buffer_size);
    if (bytes_read < 0) {
        perror("Failed to read frame");
        free(buffer);
        return -1;
    }
    
    // 保存到文件（如果需要）
    if (filename) {
        int file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (file_fd == -1) {
            perror("Failed to open output file");
            free(buffer);
            return -1;
        }
        
        size_t written = 0;
        while (written < bytes_read) {
            ssize_t n = write(file_fd, buffer + written, bytes_read - written);
            if (n <= 0) {
                perror("write error");
                break;
            }
            written += n;
        }
        
        close(file_fd);
        
        if (written != bytes_read) {
            fprintf(stderr, "Incomplete write: %zu of %zd bytes\n", written, bytes_read);
            free(buffer);
            return -1;
        }
    }
    
    free(buffer);
    return 0;
}

int capture_image(int fd, Buffer* buffers, int index, const char *filename) {
    struct v4l2_buffer buf = {0};
    
    // 入队缓冲区
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;
    if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
        perror("QBUF failed");
        return -1;
    }

    // 等待帧捕获完成
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv = {.tv_sec = 2, .tv_usec = 0};
    
    int r = select(fd + 1, &fds, NULL, NULL, &tv);
    if (r <= 0) {
        perror("select timeout or error");
        return -1;
    }

    // 出队缓冲区
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
        perror("DQBUF failed");
        return -1;
    }

    // 保存到文件（如果需要）
    if (filename) {
        int file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (file_fd == -1) {
            perror("Failed to open output file");
            return -1;
        }
        
        size_t written = 0;
        size_t total = buf.bytesused;
        while (written < total) {
            ssize_t n = write(file_fd, buffers[buf.index].start + written, total - written);
            if (n <= 0) {
                perror("write error");
                break;
            }
            written += n;
        }
        
        close(file_fd);
        
        if (written != total) {
            fprintf(stderr, "Incomplete write: %zu of %zu bytes\n", written, total);
            return -1;
        }
    }

    return buf.index;
}

double benchmark_single_frame_capture(int fd) {
    uint64_t min_ns = UINT64_MAX;
    uint64_t max_ns = 0;
    double total_ns = 0;
    int success_count = 0;
    double max_freq_hz = 0;
    char filename[32];
    int save_counter = 0;
    
    // 预热运行
    printf("Warming up (single frame mode)...\n");
    for (int i = 0; i < WARMUP_RUNS; i++) {
        int result = capture_single_frame(fd, NULL);
        if (result < 0) break;
    }

    // 主测试循环
    printf("Starting benchmark (saving every %d frames to conserve /dev/shm space)...\n", SAVE_INTERVAL);
    for (int run = 0; run < MAX_RUNS; run++) {
        uint64_t start = get_time_ns();
        
        // 只在特定间隔保存文件，并限制文件数量
        char *save_filename = NULL;
        if (save_counter % SAVE_INTERVAL == 0) {
            snprintf(filename, sizeof(filename), FILENAME_TEMPLATE, (save_counter / SAVE_INTERVAL) % 3 + 1);
            save_filename = filename;
        }
        save_counter++;
        
        int result = capture_single_frame(fd, save_filename);
        
        if (result < 0) {
            fprintf(stderr, "Capture failed on run %d\n", run+1);
            continue;
        }
        
        uint64_t end = get_time_ns();
        uint64_t duration = end - start;
        
        // 更新统计数据
        if (duration < min_ns) min_ns = duration;
        if (duration > max_ns) max_ns = duration;
        total_ns += duration;
        success_count++;
        
        // 计算当前频率
        double freq_hz = 1e9 / (double)duration;
        if (freq_hz > max_freq_hz) max_freq_hz = freq_hz;
        
        // 实时输出
        printf("Run %03d: %5.2f ms (%5.2f Hz)%s\r", 
               run+1, (double)duration/1e6, freq_hz,
               save_filename ? " [SAVED]" : "");
        fflush(stdout);
    }
    
    // 计算结果
    if (success_count == 0) return 0.0;
    
    double avg_ns = total_ns / success_count;
    double avg_freq_hz = 1e9 / avg_ns;
    double min_freq_hz = 1e9 / (double)min_ns;
    double max_freq_hz_calc = 1e9 / (double)max_ns;
    
    // 输出最终报告
    printf("\n\n=== Benchmark Results (Single Frame Mode) ===\n");
    printf("Successful runs:    %d/%d\n", success_count, MAX_RUNS);
    printf("Min execution time: %7.3f ms (%6.2f Hz)\n", (double)min_ns/1e6, min_freq_hz);
    printf("Avg execution time: %7.3f ms (%6.2f Hz)\n", avg_ns/1e6, avg_freq_hz);
    printf("Max execution time: %7.3f ms (%6.2f Hz)\n", (double)max_ns/1e6, max_freq_hz_calc);
    printf("Peak frequency:     %6.2f Hz\n", max_freq_hz);
    printf("Files saved:        %d (rotating in /dev/shm)\n", (save_counter + SAVE_INTERVAL - 1) / SAVE_INTERVAL);
    
    return min_freq_hz;  // 返回最高频率
}

int main() {
    const char *dev_name = "/dev/video0";
    int fd = -1;
    Buffer* buffers = NULL;
    struct v4l2_format fmt = {0};
    struct v4l2_requestbuffers req = {0};
    struct v4l2_capability cap = {0};
    int buffer_count = BUFFER_COUNT;
    
    // 打开设备
    fd = open(dev_name, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return EXIT_FAILURE;
    }

    // 查询设备能力
    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        perror("QUERYCAP failed");
        close(fd);
        return EXIT_FAILURE;
    }
    
    printf("Device: %s\n", cap.card);
    printf("Driver: %s\n", cap.driver);
    printf("Capabilities: 0x%08x\n", cap.capabilities);
    
    // 检查是否支持读写或者streaming
    int use_streaming = 0;
    if (cap.capabilities & V4L2_CAP_STREAMING) {
        printf("Device supports streaming mode\n");
        use_streaming = 1;
    } else if (cap.capabilities & V4L2_CAP_READWRITE) {
        printf("Device supports read/write mode (single frame capture)\n");
        use_streaming = 0;
    } else {
        fprintf(stderr, "Device does not support any capture method\n");
        close(fd);
        return EXIT_FAILURE;
    }

    // 枚举支持的格式
    enumerate_formats(fd);

    // 获取当前格式
    struct v4l2_format current_fmt = {0};
    current_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd, VIDIOC_G_FMT, &current_fmt) == -1) {
        perror("VIDIOC_G_FMT failed");
    } else {
        printf("\nCurrent format:\n");
        print_format_info(&current_fmt);
    }

    // 尝试设置我们想要的格式
    printf("\nAttempting to set format to %dx%d (0x%08x)...\n", WIDTH, HEIGHT, PIXELFORMAT);

    // 设置视频格式
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = PIXELFORMAT;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    
    if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        fprintf(stderr, "Failed to set format %dx%d with pixelformat 0x%08x: %s\n", 
                WIDTH, HEIGHT, PIXELFORMAT, strerror(errno));
        
        // 尝试常见格式
        if (try_common_formats(fd, &fmt) == -1) {
            fprintf(stderr, "All format attempts failed\n");
            close(fd);
            return EXIT_FAILURE;
        }
    } else {
        printf("Successfully set requested format!\n");
    }
    
    printf("Using format:\n");
    print_format_info(&fmt);
    
    // 计算并显示每帧大小
    size_t frame_size = fmt.fmt.pix.sizeimage;
    if (frame_size == 0) {
        // 手动计算帧大小（对于10位数据）
        frame_size = WIDTH * HEIGHT * 10 / 8;
    }
    printf("Frame size: %zu bytes (%.2f MB)\n", frame_size, frame_size / (1024.0 * 1024.0));
    printf("Note: /dev/shm has limited space (~16.5MB), files will be rotated\n");

    double max_freq;
    
    if (use_streaming) {
        // 使用流模式
        // 请求缓冲区
        req.count = BUFFER_COUNT;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        
        if (xioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
            perror("REQBUFS failed");
            close(fd);
            return EXIT_FAILURE;
        }

        if (req.count < 1) {
            fprintf(stderr, "Insufficient buffer memory\n");
            close(fd);
            return EXIT_FAILURE;
        }
        buffer_count = req.count;
        printf("Using %d buffers\n", buffer_count);

        // 分配缓冲区数组
        buffers = calloc(buffer_count, sizeof(Buffer));
        if (!buffers) {
            perror("Failed to allocate buffer array");
            close(fd);
            return EXIT_FAILURE;
        }

        // 映射缓冲区
        for (int i = 0; i < buffer_count; i++) {
            struct v4l2_buffer buf = {0};
            
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            
            if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
                perror("QUERYBUF failed");
                goto cleanup;
            }
            
            buffers[i].length = buf.length;
            buffers[i].start = mmap(NULL, buf.length, 
                                    PROT_READ | PROT_WRITE, 
                                    MAP_SHARED, 
                                    fd, buf.m.offset);
            
            if (buffers[i].start == MAP_FAILED) {
                perror("mmap failed");
                goto cleanup;
            }
        }

        // 将缓冲区加入队列
        for (int i = 0; i < buffer_count; i++) {
            struct v4l2_buffer buf = {0};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            
            if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
                perror("QBUF initial failed");
                goto cleanup;
            }
        }

        // 开始视频流
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
            perror("STREAMON failed");
            goto cleanup;
        }

        // 执行基准测试
        max_freq = benchmark_single_frame_capture(fd);
        
        // 停止视频流
        enum v4l2_buf_type stop_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd, VIDIOC_STREAMOFF, &stop_type);
        
    } else {
        // 使用单帧读取模式
        printf("Using single frame capture mode (read/write)\n");
        max_freq = benchmark_single_frame_capture(fd);
    }
    
    printf("Maximum frequency: %.2f Hz\n", max_freq);

cleanup:
    // 清理资源
    if (buffers) {
        for (int i = 0; i < buffer_count; i++) {
            if (buffers[i].start) {
                munmap(buffers[i].start, buffers[i].length);
            }
        }
        free(buffers);
    }
    
    if (fd != -1) {
        close(fd);
    }

    return EXIT_SUCCESS;
}