#!/usr/bin/env bash
set -euo pipefail

# STM32F103C8T6 LED 闪灯 —— CMake + Ninja + arm-none-eabi-gcc 构建脚本。
# 前提：已用 STM32CubeMX 以 "Toolchain/IDE = CMake" 生成本目录工程，
#       且已安装 arm-none-eabi-gcc / newlib / cmake / ninja（见 README 第二节）。
# 注意：不要用 sudo 运行本脚本。

cd "$(dirname "$0")"

if [[ ! -f CMakeLists.txt ]]; then
  echo "错误：当前目录没有 CMakeLists.txt。" >&2
  echo "请先用 STM32CubeMX（Toolchain/IDE = CMake）把工程生成到这里。" >&2
  echo "详见 README.md 第三节。" >&2
  exit 1
fi

if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
  echo "错误：未找到 arm-none-eabi-gcc。" >&2
  echo "请先安装：sudo pacman -S arm-none-eabi-gcc arm-none-eabi-newlib" >&2
  exit 1
fi

BUILD_TYPE="${1:-Debug}"   # 用法: ./build.sh [Debug|Release]

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
cmake --build build -j "$(nproc)"

echo
echo "=== 构建完成，产物大小 ==="
if command -v arm-none-eabi-size >/dev/null 2>&1; then
  arm-none-eabi-size build/*.elf 2>/dev/null || true
fi
ls -1 build/*.elf 2>/dev/null || echo "(未找到 .elf，请检查 CubeMX 工程名与构建输出)"
