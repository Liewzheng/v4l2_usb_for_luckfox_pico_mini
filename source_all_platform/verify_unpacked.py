#!/usr/bin/env python3
"""
验证C语言解包算法的正确性和显示解包后的图像

用法：
python verify_unpacked.py frame_000001_1920x1080_unpacked.raw 1920 1080

这个脚本将读取C程序输出的解包数据，并与Python版本的解包算法进行对比验证。
"""

import sys
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

def load_unpacked_image(filename, width, height):
    """
    加载C程序解包后的16位图像数据
    
    Args:
        filename: 解包后的.raw文件路径
        width: 图像宽度
        height: 图像高度
    
    Returns:
        numpy数组，形状为(height, width)，dtype=uint16
    """
    try:
        # 读取16位小端序数据
        data = np.fromfile(filename, dtype=np.uint16)
        expected_pixels = width * height
        
        if len(data) != expected_pixels:
            print(f"Warning: Expected {expected_pixels} pixels, got {len(data)}")
        
        # 重整为图像形状
        image = data[:expected_pixels].reshape(height, width)
        return image
    except Exception as e:
        print(f"Error loading unpacked image: {e}")
        return None

def python_unpack_sbggr10(raw_data):
    """
    Python版本的SBGGR10解包算法（用于验证）
    """
    img = []
    
    for i in range(0, len(raw_data), 5):
        if i + 4 >= len(raw_data):
            break
            
        # 重构40位数据
        pixels_bin = f"{raw_data[i+4]:08b}{raw_data[i+3]:08b}{raw_data[i+2]:08b}{raw_data[i+1]:08b}{raw_data[i+0]:08b}"
        
        px1 = int(pixels_bin[0:10], 2)   # 前10位
        px2 = int(pixels_bin[10:20], 2)  # 中间10位
        px3 = int(pixels_bin[20:30], 2)  # 后10位
        px4 = int(pixels_bin[30:40], 2)  # 最后10位
        
        img.extend([px1, px2, px3, px4])
    
    return np.array(img, dtype=np.uint16)

def verify_unpacking(raw_filename, unpacked_filename):
    """
    验证C语言解包结果与Python解包结果的一致性
    """
    print("Verifying unpacking algorithm...")
    
    # 读取原始RAW数据
    try:
        raw_data = np.fromfile(raw_filename, dtype=np.uint8)
        print(f"Loaded RAW data: {len(raw_data)} bytes")
    except:
        print(f"Warning: Could not load RAW file {raw_filename} for verification")
        return True  # 跳过验证
    
    # Python解包
    python_result = python_unpack_sbggr10(raw_data)
    print(f"Python unpacked: {len(python_result)} pixels")
    
    # 读取C语言解包结果
    try:
        c_result = np.fromfile(unpacked_filename, dtype=np.uint16)
        print(f"C unpacked: {len(c_result)} pixels")
    except:
        print(f"Error: Could not load C unpacked file {unpacked_filename}")
        return False
    
    # 比较结果
    min_len = min(len(python_result), len(c_result))
    if min_len == 0:
        print("Error: No data to compare")
        return False
    
    python_result = python_result[:min_len]
    c_result = c_result[:min_len]
    
    if np.array_equal(python_result, c_result):
        print("✓ Verification PASSED: C and Python results are identical")
        return True
    else:
        diff_count = np.sum(python_result != c_result)
        print(f"✗ Verification FAILED: {diff_count}/{min_len} pixels differ")
        
        # 显示前几个差异
        diff_indices = np.where(python_result != c_result)[0][:10]
        for idx in diff_indices:
            print(f"  Pixel {idx}: Python={python_result[idx]}, C={c_result[idx]}")
        
        return False

def display_image(image, title="Unpacked Image"):
    """
    显示解包后的图像
    """
    plt.figure(figsize=(12, 8))
    
    # 显示原图（缩放到8位用于显示）
    plt.subplot(221)
    display_img = (image >> 2).astype(np.uint8)  # 10位->8位
    plt.imshow(display_img, cmap='gray')
    plt.title(f'{title} (10->8 bit)')
    plt.colorbar()
    
    # 显示直方图
    plt.subplot(222)
    plt.hist(image.flatten(), bins=256, alpha=0.7)
    plt.title('Pixel Value Histogram')
    plt.xlabel('Pixel Value (10-bit)')
    plt.ylabel('Count')
    
    # 显示图像统计
    plt.subplot(223)
    stats_text = f"""
Image Statistics:
Size: {image.shape[1]}x{image.shape[0]}
Min: {image.min()}
Max: {image.max()}
Mean: {image.mean():.1f}
Std: {image.std():.1f}
"""
    plt.text(0.1, 0.5, stats_text, fontsize=10, family='monospace')
    plt.axis('off')
    
    # 显示部分区域的放大图
    plt.subplot(224)
    h, w = image.shape
    crop = image[h//4:h//4+100, w//4:w//4+100]
    plt.imshow((crop >> 2).astype(np.uint8), cmap='gray')
    plt.title('Cropped Region (100x100)')
    
    plt.tight_layout()
    plt.show()

def main():
    if len(sys.argv) != 4:
        print("Usage: python verify_unpacked.py <unpacked_file> <width> <height>")
        print("Example: python verify_unpacked.py frame_000001_1920x1080_unpacked.raw 1920 1080")
        sys.exit(1)
    
    unpacked_file = sys.argv[1]
    width = int(sys.argv[2])
    height = int(sys.argv[3])
    
    print(f"Loading unpacked image: {unpacked_file}")
    print(f"Expected dimensions: {width}x{height}")
    
    # 加载解包后的图像
    image = load_unpacked_image(unpacked_file, width, height)
    if image is None:
        sys.exit(1)
    
    print(f"Loaded image shape: {image.shape}")
    print(f"Data type: {image.dtype}")
    print(f"Value range: {image.min()} - {image.max()}")
    
    # 验证解包算法（如果有对应的RAW文件）
    raw_file = unpacked_file.replace('_unpacked.raw', '.BG10')
    if Path(raw_file).exists():
        verify_unpacking(raw_file, unpacked_file)
    
    # 显示图像
    display_image(image, f"Frame {width}x{height}")
    
    print("\nImage processing completed!")
    print("Note: The displayed image is converted from 10-bit to 8-bit for visualization.")
    print("For accurate analysis, use the original 16-bit data.")

if __name__ == "__main__":
    main()
