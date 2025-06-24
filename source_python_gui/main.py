#!/usr/bin/env python3
"""
V4L2 USB RAW Image Receiver - PySide6 GUI Client
==============================================

简化的 GUI 客户端，通过 ctypes 调用 C 动态库接口。

Features:
- Real-time Bayer RAW image display via C library
- FPS monitoring
- File save path configuration
- Start/Stop reception control
- Simplified ctypes interface

Author: Development Team
Date: 2025-06-24
Version: 2.0.0
"""

import sys
import os
import time
import ctypes
from ctypes import POINTER, Structure, c_char_p, c_int, c_uint32, c_void_p, c_bool
from pathlib import Path
from typing import Optional
import threading

import numpy as np
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QVBoxLayout, QHBoxLayout, 
    QWidget, QPushButton, QLabel, QLineEdit, QTextEdit,
    QFileDialog, QGroupBox, QGridLayout, QStatusBar
)
from PySide6.QtCore import QThread, Signal, QTimer, Qt
from PySide6.QtGui import QPixmap, QImage

# ========================== 常量定义 ==========================

# 默认配置
DEFAULT_SERVER_IP = "172.32.0.93"
DEFAULT_PORT = 8888

# 图像参数
DEFAULT_WIDTH = 1920
DEFAULT_HEIGHT = 1080

# UI 配置
UPDATE_INTERVAL = 100  # 界面更新间隔 (ms)

# ========================== 通用类定义 ==========================

class FrameInfo:
    """帧信息类（Python 侧使用）"""
    def __init__(self):
        self.frame_id = 0
        self.width = DEFAULT_WIDTH
        self.height = DEFAULT_HEIGHT
        self.timestamp = 0
        self.fps = 0
        self.total_frames = 0
        self.is_connected = False

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

class V4L2Client:
    """V4L2 客户端 C 库接口封装"""
    
    def __init__(self):
        self.lib = None
        self.config = None
        self.sock_fd = None
        self.frame_buffer = None
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
                    break
            
            if not self.lib:
                raise FileNotFoundError(f"找不到 C 库文件: {lib_name}")
            
            # 定义函数原型
            self.setup_function_prototypes()
            print(f"成功加载 C 库: {lib_name}")
            
        except Exception as e:
            print(f"加载 C 库失败: {e}")
            # 使用模拟模式
            self.lib = None
    
    def setup_function_prototypes(self):
        """设置 C 函数原型"""
        if not self.lib:
            return
        
        # 使用现有的 C 函数接口
        
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
        
        # 获取全局统计信息（需要通过指针访问）
        try:
            self.stats_ptr = ctypes.cast(self.lib.stats, POINTER(Stats))
        except:
            self.stats_ptr = None
        
        # 获取全局解包缓冲区访问
        try:
            # g_unpack_buffer 和 g_buffer_size 的访问
            self.g_unpack_buffer_ptr = ctypes.cast(self.lib.g_unpack_buffer, POINTER(ctypes.POINTER(ctypes.c_uint16)))
            self.g_buffer_size_ptr = ctypes.cast(self.lib.g_buffer_size, POINTER(ctypes.c_size_t))
            print("✅ 成功访问全局解包缓冲区")
        except Exception as e:
            print(f"⚠️  无法访问全局解包缓冲区: {e}")
            self.g_unpack_buffer_ptr = None
            self.g_buffer_size_ptr = None
    
    def create_client(self, server_ip: str, port: int, save_path: Optional[str] = None):
        """创建客户端"""
        if not self.lib:
            return False
        
        # 初始化网络
        if self.lib.init_network() != 0:
            print("网络初始化失败")
            return False
        
        # 初始化内存池
        self.lib.init_memory_pool()
        
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
            if self.lib.create_output_dir(save_path.encode('utf-8')) != 0:
                print(f"无法创建输出目录: {save_path}")
                return False
        
        return True
    
    def start(self):
        """开始接收"""
        if not self.lib or not self.config:
            return False
        
        # 连接到服务器
        self.sock_fd = self.lib.connect_to_server(self.config.server_ip, self.config.port)
        if self.sock_fd < 0:
            print("连接服务器失败")
            return False
        
        print(f"已连接到服务器: {self.config.server_ip.decode()}:{self.config.port}")
        return True
    
    def stop(self):
        """停止接收"""
        # 设置全局运行标志为停止
        if self.lib:
            try:
                # 通过访问 running 变量来停止
                running_ptr = ctypes.cast(self.lib.running, POINTER(c_int))
                running_ptr.contents = c_int(0)
            except:
                pass
    
    def destroy(self):
        """销毁客户端"""
        if not self.lib:
            return
        
        # 清理内存池
        self.lib.cleanup_memory_pool()
        
        # 清理网络
        self.lib.cleanup_network()
        
        self.config = None
        self.sock_fd = None
    
    def get_frame(self) -> Optional[np.ndarray]:
        """获取最新的 16-bit 图像数据"""
        if not self.lib or not self.g_unpack_buffer_ptr or not self.g_buffer_size_ptr:
            return None
        
        try:
            # 获取缓冲区指针和大小
            buffer_ptr = self.g_unpack_buffer_ptr.contents
            buffer_size = self.g_buffer_size_ptr.contents.value
            
            if not buffer_ptr or buffer_size == 0:
                return None
            
            # 将 C 缓冲区数据复制到 numpy 数组
            # 假设图像是 DEFAULT_WIDTH x DEFAULT_HEIGHT 的 16-bit 数据
            expected_pixels = DEFAULT_WIDTH * DEFAULT_HEIGHT
            if buffer_size < expected_pixels:
                return None
            
            # 创建 numpy 数组从 C 缓冲区
            c_array = ctypes.cast(buffer_ptr, ctypes.POINTER(ctypes.c_uint16 * expected_pixels))
            raw_data = np.frombuffer(c_array.contents, dtype=np.uint16).copy()
            
            # 重塑为图像形状
            image_16bit = raw_data[:expected_pixels].reshape((DEFAULT_HEIGHT, DEFAULT_WIDTH))
            
            # 转换为显示格式（8-bit RGB）
            return self.convert_16bit_to_display(image_16bit)
            
        except Exception as e:
            print(f"获取帧数据失败: {e}")
            return None
    
    def convert_16bit_to_display(self, image_16bit: np.ndarray) -> np.ndarray:
        """将 16-bit RAW 图像转换为显示用的 8-bit RGB 图像 (类似 ImageJ)"""
        # 统计信息
        min_val = np.min(image_16bit)
        max_val = np.max(image_16bit)
        mean_val = np.mean(image_16bit)
        std_val = np.std(image_16bit)
        
        print(f"RAW 图像统计: Min={min_val}, Max={max_val}, Mean={mean_val:.1f}, Std={std_val:.1f}")
        
        if max_val == min_val:
            # 如果图像没有动态范围，返回黑色图像
            normalized = np.zeros_like(image_16bit, dtype=np.uint8)
        else:
            # 自动对比度调整：使用 1% 和 99% 分位数进行拉伸
            p1 = np.percentile(image_16bit, 1)
            p99 = np.percentile(image_16bit, 99)
            
            if p99 > p1:
                # 裁剪到分位数范围并拉伸
                clipped = np.clip(image_16bit, p1, p99)
                normalized = ((clipped - p1) / (p99 - p1) * 255).astype(np.uint8)
            else:
                # 使用全范围拉伸
                normalized = ((image_16bit - min_val) / (max_val - min_val) * 255).astype(np.uint8)
        
        # 可选：应用伪彩色映射
        if hasattr(self, 'use_false_color') and self.use_false_color:
            return self.apply_false_color(normalized)
        else:
            # 灰度 RGB
            rgb_image = np.stack([normalized, normalized, normalized], axis=2)
            return rgb_image
    
    def apply_false_color(self, gray_image: np.ndarray) -> np.ndarray:
        """应用伪彩色映射 (热图风格)"""
        # 简单的热图映射
        rgb_image = np.zeros((gray_image.shape[0], gray_image.shape[1], 3), dtype=np.uint8)
        
        # 红色通道
        rgb_image[:, :, 0] = np.clip(gray_image * 1.5 - 128, 0, 255).astype(np.uint8)
        
        # 绿色通道
        rgb_image[:, :, 1] = np.clip(np.abs(gray_image - 128) * 2, 0, 255).astype(np.uint8)
        
        # 蓝色通道
        rgb_image[:, :, 2] = np.clip(255 - gray_image * 1.5, 0, 255).astype(np.uint8)
        
        return rgb_image
    
    def get_info(self) -> Optional[FrameInfo]:
        """获取帧信息"""
        if not self.lib or not self.stats_ptr:
            return None
        
        try:
            stats = self.stats_ptr.contents
            # 创建帧信息对象
            info = FrameInfo()
            info.frame_id = stats.frames_received
            info.width = DEFAULT_WIDTH
            info.height = DEFAULT_HEIGHT
            info.timestamp = int(time.time() * 1000)
            info.fps = int(stats.avg_fps)
            info.total_frames = stats.frames_received
            info.is_connected = True
            
            return info
        except:
            return None
    
    def run_receive_loop(self):
        """运行接收循环（在线程中调用）"""
        if not self.lib or not self.config or self.sock_fd is None:
            return False
        
        # 调用 C 的接收循环
        result = self.lib.receive_loop(self.sock_fd, ctypes.byref(self.config))
        return result == 0

# ========================== 模拟数据生成器 ==========================

class MockDataGenerator:
    """模拟数据生成器（当 C 库不可用时）"""
    
    def __init__(self):
        self.frame_count = 0
        self.start_time = time.time()
        self.running = False
    
    def create_client(self, server_ip: str, port: int, save_path: Optional[str] = None):
        """模拟创建客户端"""
        print(f"模拟模式: 连接到 {server_ip}:{port}")
        return True
    
    def start(self):
        """模拟开始"""
        self.running = True
        self.start_time = time.time()
        return True
    
    def stop(self):
        """模拟停止"""
        self.running = False
    
    def destroy(self):
        """模拟销毁"""
        pass
    
    def get_frame(self) -> Optional[np.ndarray]:
        """生成模拟帧数据"""
        if not self.running:
            return None
        
        # 生成简单的渐变图像
        frame = np.zeros((DEFAULT_HEIGHT, DEFAULT_WIDTH, 3), dtype=np.uint8)
        
        # 创建时间相关的动画效果
        t = time.time() - self.start_time
        offset = int(t * 50) % 256
        
        # 生成渐变图案
        for y in range(DEFAULT_HEIGHT):
            for x in range(DEFAULT_WIDTH):
                frame[y, x, 0] = (x + offset) % 256  # Red
                frame[y, x, 1] = (y + offset) % 256  # Green
                frame[y, x, 2] = ((x + y + offset) // 2) % 256  # Blue
        
        self.frame_count += 1
        return frame
    
    def get_info(self) -> Optional[FrameInfo]:
        """生成模拟帧信息"""
        if not self.running:
            return None
        
        elapsed = time.time() - self.start_time
        fps = self.frame_count / elapsed if elapsed > 0 else 0
        
        info = FrameInfo()
        info.frame_id = self.frame_count
        info.width = DEFAULT_WIDTH
        info.height = DEFAULT_HEIGHT
        info.timestamp = int(time.time() * 1000)
        info.fps = int(fps)
        info.total_frames = self.frame_count
        info.is_connected = True
        
        return info

# ========================== 数据接收线程 ==========================

class DataReceiver(QThread):
    """数据接收线程"""
    
    frame_updated = Signal(np.ndarray)     # 新帧数据
    info_updated = Signal(object)          # 帧信息更新
    error_occurred = Signal(str)           # 错误信息
    
    def __init__(self, server_ip: str, port: int, save_path: Optional[str] = None):
        super().__init__()
        self.server_ip = server_ip
        self.port = port
        self.save_path = save_path
        self.running = False
        
        # 使用类似 test_client.py 的方式
        self.client = V4L2Client()
        
        # 如果 C 库不可用，使用模拟器
        if not self.client.lib:
            self.client = MockDataGenerator()
            print("使用模拟模式")
        else:
            print("使用 C 库模式")
    
    def start_receiving(self):
        """开始接收"""
        self.running = True
        self.start()
    
    def stop_receiving(self):
        """停止接收"""
        self.running = False
        if self.client:
            self.client.stop()
        self.wait()
    
    def run(self):
        """主循环"""
        try:
            # 创建客户端连接
            if not self.client.create_client(self.server_ip, self.port, self.save_path):
                self.error_occurred.emit("无法创建客户端连接")
                return
            
            # 开始接收
            if not self.client.start():
                self.error_occurred.emit("无法开始接收数据")
                return
            
            # 检查是否是真实的 C 库客户端
            if hasattr(self.client, 'lib') and self.client.lib:
                # 运行 C 库的接收循环（这是一个阻塞操作）
                print("启动 C 库接收循环...")
                
                # 在单独的线程中运行 C 接收循环
                from threading import Thread
                
                def run_c_receive():
                    try:
                        self.client.run_receive_loop()
                    except Exception as e:
                        print(f"C 库接收循环错误: {e}")
                
                c_thread = Thread(target=run_c_receive, daemon=True)
                c_thread.start()
                
                # 主循环：监控统计信息并获取真实图像
                while self.running:
                    # 获取统计信息
                    info = self.client.get_info()
                    if info is not None:
                        self.info_updated.emit(info)
                        
                        # 尝试获取真实的 16-bit 图像数据
                        frame = self.client.get_frame()
                        if frame is not None:
                            self.frame_updated.emit(frame)
                        else:
                            # 如果无法获取真实数据，生成状态显示图像
                            demo_frame = self.generate_demo_frame(info)
                            if demo_frame is not None:
                                self.frame_updated.emit(demo_frame)
                    
                    # 控制更新频率
                    self.msleep(100)  # 10 FPS for GUI updates
                
            else:
                # 使用模拟器的接收循环
                while self.running:
                    # 获取帧数据
                    frame = self.client.get_frame()
                    if frame is not None:
                        self.frame_updated.emit(frame)
                    
                    # 获取帧信息
                    info = self.client.get_info()
                    if info is not None:
                        self.info_updated.emit(info)
                    
                    # 控制帧率
                    self.msleep(33)  # ~30 FPS
        
        except Exception as e:
            self.error_occurred.emit(f"接收线程错误: {e}")
        
        finally:
            if self.client:
                self.client.destroy()
    
    def generate_demo_frame(self, info: FrameInfo) -> Optional[np.ndarray]:
        """生成演示图像（显示接收状态）"""
        # 创建一个简单的状态显示图像
        frame = np.zeros((DEFAULT_HEIGHT, DEFAULT_WIDTH, 3), dtype=np.uint8)
        
        # 创建动态效果
        t = time.time()
        offset = int(t * 30) % 256
        
        # 背景渐变
        for y in range(DEFAULT_HEIGHT):
            for x in range(DEFAULT_WIDTH):
                # 创建基于统计信息的视觉效果
                frame[y, x, 0] = (x + info.frame_id + offset) % 256  # Red
                frame[y, x, 1] = (y + info.total_frames // 10) % 256  # Green  
                frame[y, x, 2] = ((x + y + info.fps * 2) // 2 + offset) % 256  # Blue
        
        # 添加文本信息区域（简单的亮度变化表示）
        # 在图像上部添加状态条
        status_height = 100
        frame[:status_height, :, :] = frame[:status_height, :, :] // 2 + 64
        
        return frame

# ========================== 主窗口类 ==========================

class MainWindow(QMainWindow):
    """主窗口类"""
    
    def __init__(self):
        super().__init__()
        self.receiver = None
        self.current_frame = None
        self.use_false_color = False  # 初始化显示模式
        self.init_ui()
        self.setup_timer()
    
    def init_ui(self):
        """初始化用户界面"""
        self.setWindowTitle("V4L2 USB RAW Image Receiver - GUI v2.0")
        self.setGeometry(100, 100, 1000, 700)
        
        # 创建中央部件
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)
        
        # 控制面板
        control_panel = self.create_control_panel()
        layout.addWidget(control_panel)
        
        # 图像显示
        image_panel = self.create_image_panel()
        layout.addWidget(image_panel)
        
        # 信息面板
        info_panel = self.create_info_panel()
        layout.addWidget(info_panel)
        
        # 状态栏
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("就绪")
    
    def create_control_panel(self) -> QWidget:
        """创建控制面板"""
        panel = QGroupBox("连接控制")
        layout = QGridLayout(panel)
        
        # 服务器配置
        layout.addWidget(QLabel("服务器 IP:"), 0, 0)
        self.ip_edit = QLineEdit(DEFAULT_SERVER_IP)
        layout.addWidget(self.ip_edit, 0, 1)
        
        layout.addWidget(QLabel("端口:"), 0, 2)
        self.port_edit = QLineEdit(str(DEFAULT_PORT))
        layout.addWidget(self.port_edit, 0, 3)
        
        # 保存路径
        layout.addWidget(QLabel("保存路径:"), 1, 0)
        self.save_path_edit = QLineEdit()
        self.save_path_edit.setPlaceholderText("留空则仅显示不保存")
        layout.addWidget(self.save_path_edit, 1, 1, 1, 2)
        
        self.browse_btn = QPushButton("浏览...")
        self.browse_btn.clicked.connect(self.browse_save_path)
        layout.addWidget(self.browse_btn, 1, 3)
        
        # 控制按钮
        button_layout = QHBoxLayout()
        
        self.start_btn = QPushButton("开始接收")
        self.start_btn.clicked.connect(self.start_receiving)
        button_layout.addWidget(self.start_btn)
        
        self.stop_btn = QPushButton("停止接收")
        self.stop_btn.clicked.connect(self.stop_receiving)
        self.stop_btn.setEnabled(False)
        button_layout.addWidget(self.stop_btn)
        
        layout.addLayout(button_layout, 2, 0, 1, 4)
        
        return panel
    
    def create_image_panel(self) -> QWidget:
        """创建图像显示面板"""
        panel = QGroupBox("实时图像显示")
        layout = QVBoxLayout(panel)
        
        # 显示控制选项
        control_layout = QHBoxLayout()
        
        # 显示模式选择
        self.gray_mode_btn = QPushButton("灰度显示")
        self.gray_mode_btn.setCheckable(True)
        self.gray_mode_btn.setChecked(True)
        self.gray_mode_btn.clicked.connect(self.toggle_display_mode)
        control_layout.addWidget(self.gray_mode_btn)
        
        self.false_color_btn = QPushButton("伪彩色")
        self.false_color_btn.setCheckable(True)
        self.false_color_btn.clicked.connect(self.toggle_display_mode)
        control_layout.addWidget(self.false_color_btn)
        
        # 对比度控制
        control_layout.addWidget(QLabel("对比度:"))
        
        self.auto_contrast_btn = QPushButton("自动")
        self.auto_contrast_btn.setCheckable(True)
        self.auto_contrast_btn.setChecked(True)
        control_layout.addWidget(self.auto_contrast_btn)
        
        control_layout.addStretch()
        layout.addLayout(control_layout)
        
        # 图像显示区域
        self.image_label = QLabel()
        self.image_label.setMinimumSize(640, 360)
        self.image_label.setStyleSheet("border: 1px solid gray; background-color: black;")
        self.image_label.setAlignment(Qt.AlignCenter)
        self.image_label.setText("等待连接...")
        self.image_label.setScaledContents(True)
        
        layout.addWidget(self.image_label)
        return panel
    
    def create_info_panel(self) -> QWidget:
        """创建信息面板"""
        panel = QGroupBox("状态信息")
        layout = QHBoxLayout(panel)
        
        # 性能信息
        perf_layout = QGridLayout()
        perf_layout.addWidget(QLabel("帧率:"), 0, 0)
        self.fps_label = QLabel("0 FPS")
        self.fps_label.setStyleSheet("font-weight: bold; color: green;")
        perf_layout.addWidget(self.fps_label, 0, 1)
        
        perf_layout.addWidget(QLabel("总帧数:"), 1, 0)
        self.frames_label = QLabel("0")
        perf_layout.addWidget(self.frames_label, 1, 1)
        
        perf_layout.addWidget(QLabel("分辨率:"), 2, 0)
        self.resolution_label = QLabel("0×0")
        perf_layout.addWidget(self.resolution_label, 2, 1)
        
        # 图像统计信息
        perf_layout.addWidget(QLabel("图像类型:"), 3, 0)
        self.image_type_label = QLabel("RAW 16-bit")
        self.image_type_label.setStyleSheet("font-weight: bold; color: blue;")
        perf_layout.addWidget(self.image_type_label, 3, 1)
        
        layout.addLayout(perf_layout)
        
        # 连接状态
        status_layout = QVBoxLayout()
        status_layout.addWidget(QLabel("连接状态:"))
        self.connection_label = QLabel("未连接")
        self.connection_label.setStyleSheet("font-weight: bold; color: red;")
        status_layout.addWidget(self.connection_label)
        
        layout.addLayout(status_layout)
        
        return panel
    
    def setup_timer(self):
        """设置更新定时器"""
        self.update_timer = QTimer()
        self.update_timer.timeout.connect(self.update_display)
        self.update_timer.start(UPDATE_INTERVAL)
    
    def browse_save_path(self):
        """浏览保存路径"""
        path = QFileDialog.getExistingDirectory(self, "选择保存目录")
        if path:
            self.save_path_edit.setText(path)
    
    def toggle_display_mode(self):
        """切换显示模式"""
        sender = self.sender()
        
        if sender == self.gray_mode_btn:
            if self.gray_mode_btn.isChecked():
                self.false_color_btn.setChecked(False)
                self.use_false_color = False
        elif sender == self.false_color_btn:
            if self.false_color_btn.isChecked():
                self.gray_mode_btn.setChecked(False)
                self.use_false_color = True
        
        # 如果当前有接收器在运行，更新其显示模式
        if self.receiver and hasattr(self.receiver.client, 'use_false_color'):
            self.receiver.client.use_false_color = getattr(self, 'use_false_color', False)
    
    def start_receiving(self):
        """开始接收"""
        if self.receiver and self.receiver.isRunning():
            return
        
        server_ip = self.ip_edit.text().strip()
        try:
            port = int(self.port_edit.text().strip())
        except ValueError:
            self.status_bar.showMessage("错误: 端口必须是数字")
            return
        
        save_path = self.save_path_edit.text().strip() or None
        
        if not server_ip:
            self.status_bar.showMessage("错误: 请输入服务器 IP 地址")
            return
        
        # 创建接收线程
        self.receiver = DataReceiver(server_ip, port, save_path)
        self.receiver.frame_updated.connect(self.on_frame_received)
        self.receiver.info_updated.connect(self.on_info_updated)
        self.receiver.error_occurred.connect(self.on_error_occurred)
        
        # 启动接收
        self.receiver.start_receiving()
        
        # 更新界面状态
        self.start_btn.setEnabled(False)
        self.stop_btn.setEnabled(True)
        self.connection_label.setText("连接中...")
        self.connection_label.setStyleSheet("font-weight: bold; color: orange;")
        
        self.status_bar.showMessage(f"正在连接到 {server_ip}:{port}")
    
    def stop_receiving(self):
        """停止接收"""
        if self.receiver:
            self.receiver.stop_receiving()
            self.receiver = None
        
        # 更新界面状态
        self.start_btn.setEnabled(True)
        self.stop_btn.setEnabled(False)
        self.connection_label.setText("未连接")
        self.connection_label.setStyleSheet("font-weight: bold; color: red;")
        
        self.image_label.setText("已停止接收")
        self.status_bar.showMessage("已停止接收")
    
    def on_frame_received(self, frame: np.ndarray):
        """处理接收到的帧"""
        self.current_frame = frame
    
    def on_info_updated(self, info):
        """更新帧信息"""
        self.fps_label.setText(f"{info.fps} FPS")
        self.frames_label.setText(str(info.total_frames))
        self.resolution_label.setText(f"{info.width}×{info.height}")
        
        if info.is_connected:
            self.connection_label.setText("已连接")
            self.connection_label.setStyleSheet("font-weight: bold; color: green;")
    
    def on_error_occurred(self, error_msg: str):
        """处理错误"""
        self.status_bar.showMessage(f"错误: {error_msg}")
        self.stop_receiving()
    
    def update_display(self):
        """更新显示"""
        if self.current_frame is not None:
            # 转换为 QImage
            height, width, channels = self.current_frame.shape
            bytes_per_line = channels * width
            
            qimage = QImage(
                self.current_frame.data,
                width, height,
                bytes_per_line,
                QImage.Format_RGB888
            )
            
            # 显示图像
            pixmap = QPixmap.fromImage(qimage)
            scaled_pixmap = pixmap.scaled(
                self.image_label.size(),
                Qt.KeepAspectRatio,
                Qt.SmoothTransformation
            )
            self.image_label.setPixmap(scaled_pixmap)
    
    def closeEvent(self, event):
        """窗口关闭事件"""
        if self.receiver:
            self.receiver.stop_receiving()
        event.accept()

# ========================== 主程序入口 ==========================

def main():
    """主程序入口"""
    app = QApplication(sys.argv)
    
    # 设置应用程序信息
    app.setApplicationName("V4L2 USB RAW Image Receiver")
    app.setApplicationVersion("2.0.0")
    
    print("V4L2 USB RAW Image Receiver - PySide6 GUI v2.0.0")
    print("=" * 50)
    print(f"Python: {sys.version}")
    print(f"NumPy: {np.__version__}")
    print()
    
    # 创建并显示主窗口
    window = MainWindow()
    window.show()
    
    # 运行应用程序
    sys.exit(app.exec())

if __name__ == "__main__":
    main()


