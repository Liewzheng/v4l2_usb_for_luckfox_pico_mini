/**
 * @file v4l2_usb_pc_main.c
 * @brief V4L2图像流跨平台PC客户端主程序
 *
 * 包含命令行参数解析和程序入口点。
 *
 * @author Development Team
 * @date 2025-06-24
 * @version 2.0
 */

#include "v4l2_usb_pc.h"
#include <signal.h>

// ========================== 命令行界面函数 ==========================

/**
 * @brief 打印程序使用说明
 */
void print_usage(const char* prog_name)
{
    printf("V4L2 USB RAW Image Receiver (Cross-Platform PC Client)\n");
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("Options:\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -s, --server IP     Server IP address (default: %s)\n", DEFAULT_SERVER_IP);
    printf("  -p, --port PORT     Server port (default: %d)\n", DEFAULT_PORT);
    printf("  -o, --output DIR    Output directory (default: %s)\n", OUTPUT_DIR);
    printf("  -c, --convert       Enable SBGGR10 to 16-bit conversion (default: disabled)\n");
    printf("  -i, --interval N    Save every Nth frame (default: 1)\n");
    printf("\nExample:\n");
    printf("  %s -s 172.32.0.93 -p 8888 -o ./frames\n", prog_name);
    printf("  %s -s 172.32.0.93 -c -i 5    # Enable conversion, save every 5th frame\n", prog_name);
    printf("\nNote: On Windows, use forward slashes or double backslashes for paths\n");
    printf("  Good: ./frames or .\\\\frames\n");
    printf("  Bad:  .\\frames\n");
}

/**
 * @brief 解析命令行参数
 */
int parse_arguments(int argc, char* argv[], struct client_config* config)
{
    // 设置默认值
    config->server_ip = DEFAULT_SERVER_IP;
    config->port = DEFAULT_PORT;
    config->output_dir = OUTPUT_DIR;
    config->enable_conversion = 0;  // 默认不启用转换
    config->save_interval = 1;

    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 1;  // 返回1表示显示帮助后退出
        }
        else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--server") == 0) {
            if (++i < argc) {
                config->server_ip = argv[i];
            } else {
                printf("Error: --server requires an IP address\n");
                return -1;
            }
        }
        else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (++i < argc) {
                config->port = atoi(argv[i]);
                if (config->port <= 0 || config->port > 65535) {
                    printf("Error: Invalid port number\n");
                    return -1;
                }
            } else {
                printf("Error: --port requires a port number\n");
                return -1;
            }
        }
        else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (++i < argc) {
                config->output_dir = argv[i];
            } else {
                printf("Error: --output requires a directory path\n");
                return -1;
            }
        }
        else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--convert") == 0) {
            config->enable_conversion = 1;
        }
        else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interval") == 0) {
            if (++i < argc) {
                config->save_interval = atoi(argv[i]);
                if (config->save_interval <= 0) {
                    printf("Error: Invalid interval number\n");
                    return -1;
                }
            } else {
                printf("Error: --interval requires a number\n");
                return -1;
            }
        }
        else {
            printf("Error: Unknown option %s\n", argv[i]);
            print_usage(argv[0]);
            return -1;
        }
    }

    return 0;  // 成功解析
}

// ========================== 程序主函数 ==========================

/**
 * @brief 程序入口点
 */
int main(int argc, char* argv[])
{
    struct client_config config;
    
    // 解析命令行参数
    int parse_result = parse_arguments(argc, argv, &config);
    if (parse_result != 0) {
        return (parse_result == 1) ? 0 : 1;  // 1表示显示帮助，正常退出
    }

    printf("V4L2 USB RAW Image Receiver (Cross-Platform PC Client)\n");
    printf("=====================================================\n");
    printf("Server: %s:%d\n", config.server_ip, config.port);
    printf("Output: %s\n", config.output_dir);
    printf("Save interval: every %d frame(s)\n", config.save_interval);

    // 显示处理特性信息
    printf("\nImage Processing Features:\n");
    if (config.enable_conversion) {
        printf("- SBGGR10 format conversion: ENABLED\n");
        printf("- Multi-threaded processing (%d CPU cores detected)\n", get_cpu_cores());
#ifdef __AVX2__
        printf("- AVX2 SIMD optimization enabled\n");
#elif defined(__SSE2__)
        printf("- SSE2 SIMD optimization enabled\n");
#else
        printf("- Scalar processing (no SIMD acceleration)\n");
#endif
        printf("- Output: RAW files + unpacked 16-bit files for SBGGR10\n");
    } else {
        printf("- SBGGR10 format conversion: DISABLED\n");
        printf("- Output: RAW files only\n");
        printf("- Use -c option to enable conversion\n");
    }
    printf("\n");

    // 初始化网络
    if (init_network() < 0) {
        return 1;
    }

    // 设置信号处理
#ifdef _WIN32
    SetConsoleCtrlHandler(console_handler, TRUE);
#else
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#endif

    // 初始化内存池（如果启用转换）
    if (config.enable_conversion) {
        init_memory_pool();
    }

    // 创建输出目录
    if (create_output_dir(config.output_dir) < 0) {
        cleanup_network();
        cleanup_memory_pool();
        return 1;
    }

    // 连接到服务器
    sock_fd = connect_to_server(config.server_ip, config.port);
    if (sock_fd == INVALID_SOCKET_FD) {
        cleanup_network();
        cleanup_memory_pool();
        return 1;
    }

    // 主接收循环
    int result = receive_loop(sock_fd, &config);

    // 清理
    close_socket(sock_fd);
    cleanup_network();
    cleanup_memory_pool();

    // 打印最终统计
    print_stats();

    printf("Program terminated\n");
    return result;
}
