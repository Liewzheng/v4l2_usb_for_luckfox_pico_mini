#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <errno.h>

#define WIDTH 2048
#define HEIGHT 1296
#define PIXELFORMAT V4L2_PIX_FMT_SBGGR10

int xioctl(int fd, int request, void *arg) {
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}

int main() {
    const char *dev_name = "/dev/video0";
    int fd = -1;
    struct v4l2_capability cap = {0};
    struct v4l2_format fmt = {0};
    
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
    
    if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
        printf("Device supports multiplanar video capture\n");
    }
    if (cap.capabilities & V4L2_CAP_STREAMING) {
        printf("Device supports streaming\n");
    }

    // 尝试设置多平面格式
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = WIDTH;
    fmt.fmt.pix_mp.height = HEIGHT;
    fmt.fmt.pix_mp.pixelformat = PIXELFORMAT;
    fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
    fmt.fmt.pix_mp.num_planes = 1;
    
    printf("\nTrying to set multiplanar format %dx%d, pixelformat=0x%08x...\n", 
           WIDTH, HEIGHT, PIXELFORMAT);
    
    if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        printf("VIDIOC_S_FMT failed: %s\n", strerror(errno));
        
        // 尝试获取当前格式
        printf("Trying to get current format...\n");
        memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        
        if (xioctl(fd, VIDIOC_G_FMT, &fmt) == -1) {
            printf("VIDIOC_G_FMT also failed: %s\n", strerror(errno));
        } else {
            printf("Current format: %dx%d, pixelformat=0x%08x, num_planes=%d\n",
                   fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height, 
                   fmt.fmt.pix_mp.pixelformat, fmt.fmt.pix_mp.num_planes);
        }
    } else {
        printf("SUCCESS! Multiplanar format set successfully!\n");
        printf("Format: %dx%d, pixelformat=0x%08x, num_planes=%d\n",
               fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height, 
               fmt.fmt.pix_mp.pixelformat, fmt.fmt.pix_mp.num_planes);
        if (fmt.fmt.pix_mp.num_planes > 0) {
            printf("Plane 0: bytesperline=%d, sizeimage=%d\n",
                   fmt.fmt.pix_mp.plane_fmt[0].bytesperline,
                   fmt.fmt.pix_mp.plane_fmt[0].sizeimage);
        }
    }

    close(fd);
    return EXIT_SUCCESS;
}
