#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成 Trae Proxy 应用图标
设计：8:5矩形边框，右下角缺失一个和线宽一样的正方形，中间两个正菱形
"""

from PIL import Image, ImageDraw
import os

def create_trae_icon_hires(size=1024):
    """创建高分辨率图标"""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 255))
    draw = ImageDraw.Draw(img)
    
    gold = (241, 196, 15, 255)
    
    # 基于8:5比例
    unit = size // 10
    rect_w = 8 * unit
    rect_h = 5 * unit
    
    # 居中
    left = (size - rect_w) // 2
    top = (size - rect_h) // 2
    right = left + rect_w
    bottom = top + rect_h
    
    # 线宽 = 缺口大小
    line_w = unit
    
    # 绘制矩形边框（4条线，右下角断开）
    draw.rectangle([left, top, right, top + line_w], fill=gold)  # 上
    draw.rectangle([left, top, left + line_w, bottom], fill=gold)  # 左
    draw.rectangle([left, bottom - line_w, right - line_w, bottom], fill=gold)  # 下
    draw.rectangle([right - line_w, top, right, bottom - line_w], fill=gold)  # 右
    
    # 两个正菱形
    diamond_r = line_w // 2
    inner_cx = (left + right) // 2
    inner_cy = (top + bottom) // 2
    spacing = unit * 1.2
    
    for cx in [inner_cx - spacing, inner_cx + spacing]:
        draw.polygon([
            (cx, inner_cy - diamond_r),
            (cx + diamond_r, inner_cy),
            (cx, inner_cy + diamond_r),
            (cx - diamond_r, inner_cy),
        ], fill=gold)
    
    return img

def save_icons(base_name="shanghaoqi_icon"):
    # 先生成高分辨率版本
    hires = create_trae_icon_hires(1024)
    
    sizes = [16, 32, 48, 64, 128, 256, 512, 1024]
    images = []
    
    print("正在生成图标...")
    for size in sizes:
        if size == 1024:
            img = hires
        else:
            # 从高分辨率缩放，使用高质量抗锯齿
            img = hires.resize((size, size), Image.LANCZOS)
        
        img.save(f"{base_name}_{size}x{size}.png", 'PNG')
        images.append(img)
        print(f"✓ {base_name}_{size}x{size}.png")
    
    # ICO文件 - 只用256x256
    ico_img = hires.resize((256, 256), Image.LANCZOS)
    ico_img.save(f"{base_name}.ico", format='ICO')
    print(f"✓ {base_name}.ico\n✅ 完成！")

if __name__ == "__main__":
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    save_icons()
