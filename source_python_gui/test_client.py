#!/usr/bin/env python3
"""
V4L2 USB RAW Image Receiver - 简化测试客户端
================================================

不依赖 GUI，纯命令行测试 C 库接口。

Usage:
    python3 test_client.py [server_ip] [port]

Author: Development Team
Date: 2025-06-24
Version: 2.0.0
"""

import sys
import os
import time
import ctypes
from ctypes import POINTER, Structure, c_char_p, c_int, c_uint32, c_void_p, c_bool
from typing import Optional

# ========================== 常量定义 ==========================

DEFAULT_SERVER_IP = "172.32.0.93"
DEFAULT_PORT = 8888

# ========================== C 库接口定义 ==========================

class FrameHeader(Structure):
    """帧头结构体（对应 C 中的 frame_header）"""
    _fields_ = [
        ("magic", c_uint32),
        ("frame_id", c_uint32),
        ("width", c_uint32),
        ("height", c_uint32),
        ("pixfmt", c_uint32),
        ("size", c_uint32),
        ("timestamp", ctypes.c_uint64),
        ("reserved", c_uint32 * 2)
    ]

class Stats(Structure):
    """统计信息结构体（对应 C 中的 stats）"""
    _fields_ = [
        ("frames_received", c_uint32),
        ("bytes_received", c_uint32),
        ("start_time", ctypes.c_uint64),
        ("last_frame_time", ctypes.c_uint64),
        ("avg_fps", ctypes.c_double)
    ]

class ClientConfig(Structure):
    """客户端配置结构体（对应 C 中的 client_config）"""
    _fields_ = [
        ("server_ip", c_char_p),
        ("port", c_int),
        ("output_dir", c_char_p),
        ("enable_conversion", c_int),
        ("save_interval", c_int),
        ("enable_save", c_int)
    ]

class V4L2TestClient:
    """V4L2 测试客户端"""
    
    def __init__(self):
        self.lib = None
        self.config = None
        self.sock_fd = None
        self.load_library()
    
    def load_library(self):
        """加载 C 动态库"""
        try:
            # 根据平台选择库文件
            if sys.platform == "win32":
                lib_name = "libv4l2_usb_pc.dll"
            elif sys.platform == "darwin":
                lib_name = "libv4l2_usb_pc.dylib"
            else:
                lib_name = "libv4l2_usb_pc.so"
            
            # 查找库文件路径
            lib_paths = [
                os.path.join(os.path.dirname(__file__), lib_name),
                os.path.join(os.path.dirname(__file__), "..", "source_all_platform", "build_native", "dist", f"linux_x86_64", "lib", lib_name),
                os.path.join(os.path.dirname(__file__), "..", "source_all_platform", "lib", lib_name),
                lib_name  # 系统路径
            ]
            
            for lib_path in lib_paths:
                if os.path.exists(lib_path):
                    self.lib = ctypes.CDLL(lib_path)
                    print(f"✅ 成功加载 C 库: {lib_path}")
                    break
            
            if not self.lib:
                raise FileNotFoundError(f"找不到 C 库文件: {lib_name}")
            
            # 定义函数原型
            self.setup_function_prototypes()
            
        except Exception as e:
            print(f"❌ 加载 C 库失败: {e}")
            sys.exit(1)
    
    def setup_function_prototypes(self):
        """设置 C 函数原型"""
        # init_network()
        self.lib.init_network.argtypes = []
        self.lib.init_network.restype = c_int
        
        # cleanup_network()
        self.lib.cleanup_network.argtypes = []
        self.lib.cleanup_network.restype = None
        
        # connect_to_server(ip, port)
        self.lib.connect_to_server.argtypes = [c_char_p, c_int]
        self.lib.connect_to_server.restype = c_int  # socket_t
        
        # create_output_dir(dir)
        self.lib.create_output_dir.argtypes = [c_char_p]
        self.lib.create_output_dir.restype = c_int
        
        # receive_loop(sock, config)
        self.lib.receive_loop.argtypes = [c_int, POINTER(ClientConfig)]
        self.lib.receive_loop.restype = c_int
        
        # init_memory_pool()
        self.lib.init_memory_pool.argtypes = []
        self.lib.init_memory_pool.restype = None
        
        # cleanup_memory_pool()
        self.lib.cleanup_memory_pool.argtypes = []
        self.lib.cleanup_memory_pool.restype = None
        
        print("✅ 函数原型设置完成")
    
    def test_connection(self, server_ip: str, port: int, save_path: Optional[str] = None):
        """测试连接"""
        print(f"\n🔗 测试连接到 {server_ip}:{port}")
        
        # 初始化网络
        print("📡 初始化网络...")
        if self.lib.init_network() != 0:
            print("❌ 网络初始化失败")
            return False
        print("✅ 网络初始化成功")
        
        # 初始化内存池
        print("💾 初始化内存池...")
        self.lib.init_memory_pool()
        print("✅ 内存池初始化成功")
        
        # 配置客户端
        self.config = ClientConfig()
        self.config.server_ip = server_ip.encode('utf-8')
        self.config.port = port
        self.config.output_dir = save_path.encode('utf-8') if save_path else None
        self.config.enable_conversion = 1  # 启用转换
        self.config.save_interval = 1
        self.config.enable_save = 1 if save_path else 0
        
        # 如果需要保存文件，创建输出目录
        if save_path:
            print(f"📁 创建输出目录: {save_path}")
            if self.lib.create_output_dir(save_path.encode('utf-8')) != 0:
                print(f"❌ 无法创建输出目录: {save_path}")
                return False
            print("✅ 输出目录创建成功")
        
        # 连接到服务器
        print("🌐 连接到服务器...")
        self.sock_fd = self.lib.connect_to_server(self.config.server_ip, self.config.port)
        if self.sock_fd < 0:
            print("❌ 连接服务器失败")
            return False
        
        print(f"✅ 成功连接到服务器: {server_ip}:{port}")
        print(f"📊 Socket FD: {self.sock_fd}")
        
        return True
    
    def run_receive_loop(self):
        """运行接收循环"""
        if not self.lib or not self.config or self.sock_fd is None:
            print("❌ 客户端未正确初始化")
            return False
        
        print("\\n🚀 开始接收循环...")
        print("按 Ctrl+C 停止")
        
        try:
            # 调用 C 的接收循环
            result = self.lib.receive_loop(self.sock_fd, ctypes.byref(self.config))
            
            if result == 0:
                print("\\n✅ 接收循环正常结束")
                return True
            else:
                print(f"\\n❌ 接收循环异常结束，返回值: {result}")
                return False
                
        except KeyboardInterrupt:
            print("\\n⏹️  用户中断")
            return True
        except Exception as e:
            print(f"\\n❌ 接收循环异常: {e}")
            return False
    
    def cleanup(self):
        """清理资源"""
        print("\\n🧹 清理资源...")
        
        if self.lib:
            # 清理内存池
            self.lib.cleanup_memory_pool()
            print("✅ 内存池已清理")
            
            # 清理网络
            self.lib.cleanup_network()
            print("✅ 网络已清理")
        
        self.config = None
        self.sock_fd = None
        print("✅ 资源清理完成")

def main():
    """主程序入口"""
    print("V4L2 USB RAW Image Receiver - 测试客户端 v2.0.0")
    print("=" * 60)
    
    # 解析命令行参数
    server_ip = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SERVER_IP
    port = int(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_PORT
    save_path = sys.argv[3] if len(sys.argv) > 3 else None
    
    print(f"📋 配置:")
    print(f"   服务器: {server_ip}:{port}")
    print(f"   保存路径: {save_path or '仅内存模式'}")
    
    # 创建客户端
    client = V4L2TestClient()
    
    try:
        # 测试连接
        if not client.test_connection(server_ip, port, save_path):
            print("❌ 连接测试失败")
            return 1
        
        # 运行接收循环
        client.run_receive_loop()
        
    except Exception as e:
        print(f"❌ 程序异常: {e}")
        return 1
    
    finally:
        # 清理资源
        client.cleanup()
    
    print("\\n🎉 程序结束")
    return 0

if __name__ == "__main__":
    sys.exit(main())
