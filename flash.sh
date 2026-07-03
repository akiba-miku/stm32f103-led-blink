#!/usr/bin/env bash
set -euo pipefail

# STM32F103C8T6 烧录脚本 —— CMSIS-DAP/DAPLink + OpenOCD。
# 目标芯片系列：STM32F1 → target/stm32f1x.cfg
# 调试器：CMSIS-DAP/DAPLink → interface/cmsis-dap.cfg
# 注意：不要用 sudo 运行；若权限不足请配置 udev 规则（见 docs/linux-stm32-workflow.md 第5节）。

cd "$(dirname "$0")"

if ! command -v openocd >/dev/null 2>&1; then
  echo "错误：未找到 openocd。请先安装：sudo pacman -S openocd stlink" >&2
  exit 1
fi

# 自动找 build/ 下的 ELF，避免硬编码工程名。
ELF="$(ls build/*.elf 2>/dev/null | head -n1 || true)"
if [[ -z "${ELF}" ]]; then
  echo "错误：build/ 下没有 .elf。请先运行 ./build.sh 编译。" >&2
  exit 1
fi
echo "烧录: ${ELF}"

# --- 目标芯片配置 ---------------------------------------------------------
# 本工程是 STM32F103C8T6（F1 系列），所以用 stm32f1x.cfg。
# TODO: 如果你换了别的芯片系列，请改这里：
#   F0→stm32f0x.cfg  F4→stm32f4x.cfg  G0→stm32g0x.cfg  H7→stm32h7x.cfg ...
OPENOCD_INTERFACE="interface/cmsis-dap.cfg"
OPENOCD_TARGET="target/stm32f1x.cfg"
# -------------------------------------------------------------------------

openocd -f "${OPENOCD_INTERFACE}" -f "${OPENOCD_TARGET}" \
        -c "program ${ELF} verify reset exit"

echo "烧录完成。开发板应已复位运行，串口终端应每秒刷新一次无线接入系统界面。"
