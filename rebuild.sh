#!/bin/bash
# 快速重建脚本（WSL/Linux/macOS）
cd /c/Users/jing0/Desktop/week2_template
cmake -S . -B build
cmake --build build
echo "构建完成！运行命令："
echo "  ./build/breakout"
