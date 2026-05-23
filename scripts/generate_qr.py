#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
芦笋分拣机 — 二维码标签生成工具 (QR Code Generator)
--------------------------------------------------
本工具用于为每一台分拣机控制器生成专属的一对一蓝牙配对二维码。
手机端 (sorter_mini_phone) 扫描该二维码后，即可绑定唯一的广播名称，实现绝对隔离的安全直连。

支持输入：
  1. MAC 地址 (例如: 3E:9B:2F:10:AB:8C 或 10ab8c) -> 自动计算低3字节生成 "Sorter_10AB8C"
  2. 完整的广播名称 (例如: Sorter_10AB8C)

依赖库说明：
  本脚本运行需要 python-qrcode 和 pillow。如果没有安装，脚本会自动尝试为您安装。
"""

import sys
import os
import re
import subprocess

def install_dependencies():
    """自动安装所需的依赖库"""
    try:
        import qrcode
        from PIL import Image
    except ImportError:
        print("[*] 正在为您自动安装必要的二维码生成依赖库 (qrcode, pillow)...")
        try:
            subprocess.run([sys.executable, "-m", "pip", "install", "qrcode", "pillow"], check=True)
            print("[+] 依赖库安装成功！\n")
        except Exception as e:
            print(f"[!] 依赖库自动安装失败: {e}")
            print("[!] 请手动执行: pip install qrcode pillow")
            sys.exit(1)

# 执行依赖库检查与安装
install_dependencies()

import qrcode

def clean_mac_to_ble_name(input_str):
    """
    解析输入字符串，提取出唯一的 Sorter_XXXXXX 蓝牙名称
    """
    input_str = input_str.strip()
    
    # 模式1: 已经是 Sorter_XXXXXX 的格式
    if re.match(r"^Sorter_[0-9A-Fa-f]{6}$", input_str, re.IGNORECASE):
        # 统一大写
        parts = input_str.split("_")
        return f"Sorter_{parts[1].upper()}"
        
    # 模式2: 扁平 MAC 地址（6字节，如 3e9b2f10ab8c）
    flat_mac = re.sub(r"[^0-9A-Fa-f]", "", input_str)
    if len(flat_mac) == 12:
        # 取最后6位（低3字节）
        suffix = flat_mac[-6:].upper()
        return f"Sorter_{suffix}"
        
    # 模式3: 带冒号/横线的标准 MAC 地址（如 3E:9B:2F:10:AB:8C）
    # 或者是只有3字节的扁平后缀 (如 10AB8C)
    if len(flat_mac) == 6:
        return f"Sorter_{flat_mac.upper()}"
        
    # 无法解析时，退回到原样包装或报错
    print(f"[!] 警告: 输入内容 '{input_str}' 无法自动转换为标准 MAC 地址，将直接将其作为二维码内容！")
    return input_str

def main():
    if len(sys.argv) < 2:
        print("=" * 60)
        print("          芦笋分拣机唯一配对二维码生成工具          ")
        print("=" * 60)
        print("用法:")
        print("  python generate_qr.py <MAC地址 或 设备广播名>")
        print("\n示例:")
        print("  1) 输入 MAC 地址:")
        print("     python generate_qr.py 3E:9B:2F:10:AB:8C")
        print("     (自动输出: Sorter_10AB8C 二维码)")
        print("\n  2) 直接输入广播名:")
        print("     python generate_qr.py Sorter_10AB8C")
        print("=" * 60)
        
        # 默认演示
        test_input = "Sorter_DEMO88"
        print(f"\n[*] 未提供参数，默认生成测试用例: {test_input}")
        ble_name = test_input
    else:
        raw_input = sys.argv[1]
        ble_name = clean_mac_to_ble_name(raw_input)
        
    print(f"[+] 蓝牙配对名称: {ble_name}")
    
    # 确保输出目录存在
    output_dir = "qrcodes"
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
        
    output_path = os.path.join(output_dir, f"{ble_name}.png")
    
    # 1. 创建高品质二维码对象
    qr = qrcode.QRCode(
        version=1,
        error_correction=qrcode.constants.ERROR_CORRECT_H,  # 高容错率，适合标签打印磨损
        box_size=10,
        border=4,
    )
    qr.add_data(ble_name)
    qr.make(fit=True)
    
    # 2. 生成 PNG 图片文件
    img = qr.make_image(fill_color="black", back_color="white")
    img.save(output_path)
    
    print(f"[SUCCESS] 二维码图像已成功生成！")
    print(f"[SUCCESS] 物理标签图片文件路径: {os.path.abspath(output_path)}")
    print("\n" + "=" * 45)
    print("      在终端为您渲染的 ASCII 预览 (可直接扫码)      ")
    print("=" * 45)
    
    # 3. 终端尝试打印 ASCII 预览二维码，若因系统终端编码不支持则优雅跳过
    try:
        qr_preview = qrcode.QRCode(version=1, border=1)
        qr_preview.add_data(ble_name)
        qr_preview.make(fit=True)
        qr_preview.print_ascii(invert=True)
        print("=" * 45)
    except Exception:
        print("[*] (由于当前系统终端编码不支持 Unicode 二维码渲染，跳过 ASCII 终端预览)")
        print("=" * 45)
        
    print(f"提示: 请直接打开或打印 qrcodes/{ble_name}.png，并贴于外壳，用手机端 sorter_mini_phone 进行扫码绑定连接。")

if __name__ == "__main__":
    main()
