#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <linux/videodev2.h>
#include <time.h>
#include <errno.h>

#define WIDTH 2048
#define HEIGHT 1296
#define PIXELFORMAT V4L2_PIX_FMT_SBGGR10
#define BUFFER_COUNT 3
#define FILENAME_TEMPLATE "/dev/shm/raw_%02d.BG10"
#define WARMUP_RUNS 5
#define MAX_RUNS 1000

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

int capture_image(int fd, struct v4l2_buffer **buffers, int index, const char *filename) {
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
            ssize_t n = write(file_fd, buffers[buf.index]->start + written, total - written);
            if (n <= 0) break;
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

double benchmark_capture(int fd, struct v4l2_buffer **buffers) {
    uint64_t min_ns = UINT64_MAX;
    uint64_t max_ns = 0;
    double total_ns = 0;
    int success_count = 0;
    double max_freq_hz = 0;
    char filename[32];
    int file_index = 1;
    
    // 预热运行
    printf("Warming up...\n");
    for (int i = 0; i < WARMUP_RUNS; i++) {
        int idx = capture_image(fd, buffers, i % BUFFER_COUNT, NULL);
        if (idx < 0) break;
    }

    // 主测试循环
    printf("Starting benchmark...\n");
    for (int run = 0; run < MAX_RUNS; run++) {
        uint64_t start = get_time_ns();
        
        // 准备文件名（只在实际保存时使用）
        snprintf(filename, sizeof(filename), FILENAME_TEMPLATE, file_index);
        
        int buffer_index = run % BUFFER_COUNT;
        int result = capture_image(fd, buffers, buffer_index, filename);
        
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
        
        // 更新文件名索引
        file_index = (file_index % 99) + 1;
        
        // 实时输出
        printf("Run %03d: %5.2f ms (%5.2f Hz)\r", 
               run+1, (double)duration/1e6, freq_hz);
        fflush(stdout);
    }
    
    // 计算结果
    if (success_count == 0) return 0.0;
    
    double avg_ns = total_ns / success_count;
    double avg_freq_hz = 1e9 / avg_ns;
    double min_freq_hz = 1e9 / (double)min_ns;
    double max_freq_hz_calc = 1e9 / (double)max_ns;
    
    // 输出最终报告
    printf("\n\n=== Benchmark Results ===\n");
    printf("Successful runs:    %d/%d\n", success_count, MAX_RUNS);
    printf("Min execution time: %7.3f ms (%6.2f Hz)\n", (double)min_ns/1e6, min_freq_hz);
    printf("Avg execution time: %7.3f ms (%6.2f Hz)\n", avg_ns/1e6, avg_freq_hz);
    printf("Max execution time: %7.3f ms (%6.2f Hz)\n", (double)max_ns/1e6, max_freq_hz_calc);
    printf("Peak frequency:     %6.2f Hz\n", max_freq_hz);
    
    return min_freq_hz;  // 返回最高频率
}

int main() {
    const char *dev_name = "/dev/video0";
    int fd = -1;
    struct v4l2_buffer **buffers = NULL;
    struct v4l2_format fmt = {0};
    struct v4l2_requestbuffers req = {0};
    
    // 打开设备
    fd = open(dev_name, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return EXIT_FAILURE;
    }

    // 设置视频格式
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = PIXELFORMAT;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    
    if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        perror("Failed to set format");
        close(fd);
        return EXIT_FAILURE;
    }

    // 验证格式设置
    if (fmt.fmt.pix.width != WIDTH || fmt.fmt.pix.height != HEIGHT ||
        fmt.fmt.pix.pixelformat != PIXELFORMAT) {
        fprintf(stderr, "Format not supported\n");
        close(fd);
        return EXIT_FAILURE;
    }

    // 请求缓冲区
    req.count = BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    
    if (xioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        perror("REQBUFS failed");
        close(fd);
        return EXIT_FAILURE;
    }

    // 分配缓冲区数组
    buffers = calloc(req.count, sizeof(*buffers));
    if (!buffers) {
        perror("Failed to allocate buffer array");
        close(fd);
        return EXIT_FAILURE;
    }

    // 映射缓冲区
    for (int i = 0; i < req.count; i++) {
        struct v4l2_buffer buf = {0};
        
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        
        if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            perror("QUERYBUF failed");
            goto cleanup;
        }
        
        buffers[i] = malloc(sizeof(struct v4l2_buffer));
        if (!buffers[i]) {
            perror("Failed to allocate buffer");
            goto cleanup;
        }
        
        *buffers[i] = buf;
        
        buffers[i]->start = mmap(NULL, buf.length, 
                                PROT_READ | PROT_WRITE, 
                                MAP_SHARED, 
                                fd, buf.m.offset);
        
        if (buffers[i]->start == MAP_FAILED) {
            perror("mmap failed");
            goto cleanup;
        }
    }

    // 将缓冲区加入队列
    for (int i = 0; i < req.count; i++) {
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
    double max_freq = benchmark_capture(fd, buffers);
    printf("Maximum frequency: %.2f Hz\n", max_freq);

cleanup:
    // 停止视频流
    if (fd != -1) {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd, VIDIOC_STREAMOFF, &type);
        
        // 释放资源
        if (buffers) {
            for (int i = 0; i < req.count; i++) {
                if (buffers[i]) {
                    if (buffers[i]->start) munmap(buffers[i]->start, buffers[i]->length);
                    free(buffers[i]);
                }
            }
            free(buffers);
        }
        
        close(fd);
    }

    return EXIT_SUCCESS;
}