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
#define MAX_RUNS 100
#define SAVE_INTERVAL 10

#ifndef uint64_t
typedef unsigned long long uint64_t;
#endif

#ifndef UINT64_MAX
#define UINT64_MAX ((uint64_t)-1)
#endif

// 多平面缓冲区结构
struct buffer {
    void *start[VIDEO_MAX_PLANES];
    size_t length[VIDEO_MAX_PLANES];
    int num_planes;
};

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

void print_format_info_mp(struct v4l2_format *fmt) {
    printf("Multiplanar Format: %dx%d, pixelformat=0x%08x (%c%c%c%c), num_planes=%d\n",
           fmt->fmt.pix_mp.width, fmt->fmt.pix_mp.height,
           fmt->fmt.pix_mp.pixelformat,
           (fmt->fmt.pix_mp.pixelformat >> 0) & 0xFF,
           (fmt->fmt.pix_mp.pixelformat >> 8) & 0xFF,
           (fmt->fmt.pix_mp.pixelformat >> 16) & 0xFF,
           (fmt->fmt.pix_mp.pixelformat >> 24) & 0xFF,
           fmt->fmt.pix_mp.num_planes);
    
    for (int i = 0; i < fmt->fmt.pix_mp.num_planes; i++) {
        printf("  Plane %d: bytesperline=%d, sizeimage=%d\n", 
               i, 
               fmt->fmt.pix_mp.plane_fmt[i].bytesperline,
               fmt->fmt.pix_mp.plane_fmt[i].sizeimage);
    }
}

// 设置多平面格式
int set_format_mp(int fd, struct v4l2_format *fmt) {
    memset(fmt, 0, sizeof(*fmt));
    fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt->fmt.pix_mp.width = WIDTH;
    fmt->fmt.pix_mp.height = HEIGHT;
    fmt->fmt.pix_mp.pixelformat = PIXELFORMAT;
    fmt->fmt.pix_mp.field = V4L2_FIELD_NONE;
    
    printf("Setting multiplanar format %dx%d, pixelformat=0x%08x...\n",
           WIDTH, HEIGHT, PIXELFORMAT);
    
    if (xioctl(fd, VIDIOC_S_FMT, fmt) == -1) {
        perror("VIDIOC_S_FMT failed");
        return -1;
    }
    
    printf("SUCCESS! Multiplanar format set successfully!\n");
    print_format_info_mp(fmt);
    
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
    
    printf("Requested %d buffers, got %d\n", count, reqbuf.count);
    
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
        printf("Buffer %d has %d planes\n", i, buf.length);
        
        for (int p = 0; p < buf.length; p++) {
            buffers[i].length[p] = buf.m.planes[p].length;
            buffers[i].start[p] = mmap(NULL, buf.m.planes[p].length,
                                      PROT_READ | PROT_WRITE, MAP_SHARED,
                                      fd, buf.m.planes[p].m.mem_offset);
            
            if (MAP_FAILED == buffers[i].start[p]) {
                perror("mmap failed");
                return -1;
            }
            
            printf("  Plane %d: mapped %zu bytes at %p\n", 
                   p, buffers[i].length[p], buffers[i].start[p]);
        }
    }
    
    return reqbuf.count;
}

// 队列缓冲区
int queue_buffer_mp(int fd, int index) {
    struct v4l2_buffer buf = {0};
    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;
    buf.m.planes = planes;
    buf.length = VIDEO_MAX_PLANES;
    
    if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
        perror("VIDIOC_QBUF failed");
        return -1;
    }
    
    return 0;
}

// 出队缓冲区
int dequeue_buffer_mp(int fd, int *index) {
    struct v4l2_buffer buf = {0};
    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.m.planes = planes;
    buf.length = VIDEO_MAX_PLANES;
    
    if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
        perror("VIDIOC_DQBUF failed");
        return -1;
    }
    
    *index = buf.index;
    return buf.m.planes[0].bytesused;
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

// 保存图像数据
int save_frame_data(void *data, size_t size, const char *filename) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("Failed to open file for writing");
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

// 检查设备容量
int check_device_caps(int fd) {
    struct v4l2_capability cap = {0};
    
    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        perror("VIDIOC_QUERYCAP failed");
        return -1;
    }
    
    printf("Device: %s\n", cap.card);
    printf("Driver: %s\n", cap.driver);
    printf("Capabilities: 0x%08x\n", cap.capabilities);
    
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
        printf("Device does not support multiplanar video capture\n");
        return -1;
    }
    
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        printf("Device does not support streaming\n");
        return -1;
    }
    
    printf("Device supports multiplanar video capture\n");
    printf("Device supports streaming\n");
    
    return 0;
}

// 基准测试主函数
int benchmark_capture_mp(int fd) {
    struct v4l2_format fmt;
    struct buffer buffers[BUFFER_COUNT];
    int buffer_count;
    uint64_t times[MAX_RUNS];
    int saved_count = 0;
    
    // 设置格式
    if (set_format_mp(fd, &fmt) < 0) {
        return -1;
    }
    
    // 申请缓冲区
    buffer_count = request_buffers_mp(fd, buffers, BUFFER_COUNT);
    if (buffer_count < 0) {
        return -1;
    }
    
    // 队列所有缓冲区
    for (int i = 0; i < buffer_count; i++) {
        if (queue_buffer_mp(fd, i) < 0) {
            return -1;
        }
    }
    
    // 启动流
    if (start_streaming_mp(fd) < 0) {
        return -1;
    }
    
    printf("\nStarting benchmark (warmup + %d runs)...\n", MAX_RUNS);
    
    // 预热 + 基准测试
    for (int run = -WARMUP_RUNS; run < MAX_RUNS; run++) {
        uint64_t start_time = get_time_ns();
        
        // 等待数据可用
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        
        struct timeval tv = {.tv_sec = 2, .tv_usec = 0};
        int r = select(fd + 1, &fds, NULL, NULL, &tv);
        
        if (r == -1) {
            perror("select failed");
            break;
        } else if (r == 0) {
            printf("Timeout waiting for frame\n");
            break;
        }
        
        // 出队缓冲区
        int buf_index;
        int bytes_used = dequeue_buffer_mp(fd, &buf_index);
        if (bytes_used < 0) {
            break;
        }
        
        uint64_t end_time = get_time_ns();
        
        // 记录时间（跳过预热）
        if (run >= 0) {
            times[run] = end_time - start_time;
            
            // 定期保存图像
            if (run % SAVE_INTERVAL == 0 && saved_count < 5) {
                char filename[256];
                snprintf(filename, sizeof(filename), FILENAME_TEMPLATE, saved_count);
                
                if (save_frame_data(buffers[buf_index].start[0], 
                                   bytes_used, filename) == 0) {
                    printf("Saved frame %d to %s (%d bytes)\n", 
                           run, filename, bytes_used);
                    saved_count++;
                }
            }
            
            if (run % 10 == 0) {
                printf("Run %d: %.2f ms (%d bytes)\n", 
                       run, (end_time - start_time) / 1000000.0, bytes_used);
            }
        }
        
        // 重新队列缓冲区
        if (queue_buffer_mp(fd, buf_index) < 0) {
            break;
        }
    }
    
    // 停止流
    stop_streaming_mp(fd);
    
    // 统计分析
    printf("\n=== Benchmark Results ===\n");
    
    uint64_t min_time = UINT64_MAX, max_time = 0, total_time = 0;
    for (int i = 0; i < MAX_RUNS; i++) {
        if (times[i] < min_time) min_time = times[i];
        if (times[i] > max_time) max_time = times[i];
        total_time += times[i];
    }
    
    double avg_time = (double)total_time / MAX_RUNS;
    double min_ms = min_time / 1000000.0;
    double max_ms = max_time / 1000000.0;
    double avg_ms = avg_time / 1000000.0;
    double fps = 1000.0 / avg_ms;
    
    printf("Capture time (ms): min=%.2f, max=%.2f, avg=%.2f\n", min_ms, max_ms, avg_ms);
    printf("Average FPS: %.2f\n", fps);
    printf("Frames saved: %d\n", saved_count);
    
    // 清理映射
    for (int i = 0; i < buffer_count; i++) {
        for (int p = 0; p < buffers[i].num_planes; p++) {
            if (buffers[i].start[p] != MAP_FAILED) {
                munmap(buffers[i].start[p], buffers[i].length[p]);
            }
        }
    }
    
    return 0;
}

int main() {
    const char *device = "/dev/video0";
    int fd;
    
    printf("V4L2 Multiplanar Benchmark Tool for Luckfox Pico Mini B\n");
    printf("========================================================\n");
    
    // 打开设备
    fd = open(device, O_RDWR | O_NONBLOCK);
    if (fd == -1) {
        perror("Failed to open device");
        return -1;
    }
    
    printf("Opened device: %s\n", device);
    
    // 检查设备能力
    if (check_device_caps(fd) < 0) {
        close(fd);
        return -1;
    }
    
    // 运行基准测试
    int result = benchmark_capture_mp(fd);
    
    close(fd);
    
    if (result == 0) {
        printf("\nBenchmark completed successfully!\n");
    } else {
        printf("\nBenchmark failed!\n");
    }
    
    return result;
}
