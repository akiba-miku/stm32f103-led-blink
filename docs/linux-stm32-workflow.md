# Linux 下 STM32 开发完整工作流（参考）

本文是把 “Keil5 实验” 迁移到 Arch Linux 的背景说明与排错手册，配合根目录
`README.md` 使用。

## 1. 工具链对照（Keil → Linux）

| Keil µVision5 里的东西 | Linux 原生替代 |
| --- | --- |
| µVision IDE | VS Code / STM32CubeIDE / 纯命令行 |
| ARMCC / ARMClang 编译器 | `arm-none-eabi-gcc` + `arm-none-eabi-newlib` |
| 工程构建（.uvprojx） | CMake + Ninja（或 Make） |
| STM32CubeMX（生成代码） | STM32CubeMX（Linux 版，照用） |
| Download / Flash 按钮 | OpenOCD / st-flash / STM32CubeProgrammer |
| Debug（ULINK/ST-LINK） | OpenOCD + arm-none-eabi-gdb（+ Cortex-Debug） |
| Pack Installer（器件包） | CubeMX 生成的 HAL/启动文件/链接脚本 |

## 2. 为什么选 CMake 而不是 Keil 工程

- 纯文本、可进 Git、可在任意机器复现、可接 CI。
- CubeMX 6.x 起官方支持直接生成 **CMake** 工程（`Toolchain/IDE = CMake`），
  自带 `cmake/gcc-arm-none-eabi.cmake` 工具链文件，无需自己写交叉编译配置。
- 不依赖任何 Windows-only 组件。

## 3. 芯片关键参数（STM32F103C8T6）

- 内核 Cortex-M3，主频最高 72MHz。
- Flash 64KB（注意：很多 C8 实际可用到 128KB，但官方标称 64KB），SRAM 20KB。
- 启动文件应为 `startup_stm32f103xb.s`（xB = 中容量 64/128KB 系列）。
- 链接脚本 `STM32F103C8Tx_FLASH.ld`，Flash 起始 `0x08000000`。
- OpenOCD 目标配置：`target/stm32f1x.cfg`（**F1 系列专用，别用 f4x/g0x 等**）。

> ⚠️ 不要为别的系列硬编码 `target/stm32f1x.cfg`。换芯片要换 target：
> F4→`stm32f4x.cfg`，F0→`stm32f0x.cfg`，G0→`stm32g0x.cfg`，H7→`stm32h7x.cfg` …

## 4. 构建产物说明

- `build/<name>.elf`：含调试信息，OpenOCD/GDB 用它烧录+调试。
- `arm-none-eabi-objcopy -O binary x.elf x.bin`：纯二进制，给 `st-flash` / DFU 用。
- `arm-none-eabi-objcopy -O ihex x.elf x.hex`：Intel HEX，给 CubeProgrammer 用。
- `arm-none-eabi-size x.elf`：看 Flash/RAM 占用。

## 5. ST-LINK 权限（udev 规则）

普通用户访问 ST-LINK 可能因权限失败（`st-info --probe` 看不到、OpenOCD 报
`LIBUSB_ERROR_ACCESS`）。**不要用 sudo 跑 openocd/构建** 来绕过，正确做法是装
udev 规则：

- Arch 的 `stlink` 与 `openocd` 包通常已自带 udev 规则，安装后执行：

  ```bash
  sudo udevadm control --reload-rules
  sudo udevadm trigger
  ```

  然后**拔插一次** ST-LINK。

- 若仍不行，确认本用户在能访问 USB 的组（一般无需手动加），或检查
  `/usr/lib/udev/rules.d/` 下是否有 `*stlink*.rules` / `*openocd*.rules`。

> 本仓库不直接改系统文件；如需自定义 udev 规则，请你手动放到
> `/etc/udev/rules.d/` 并 reload。

## 6. 故障排查

### 编译相关
- `arm-none-eabi-gcc: command not found` → 没装工具链，见 README 第二节。
- CMake 报找不到编译器 → 确认 CubeMX 生成了 `cmake/gcc-arm-none-eabi.cmake`，
  且顶层 `CMakeLists.txt` 里 `set(CMAKE_TOOLCHAIN_FILE ...)` 指向它。
- `region 'FLASH' overflowed` → 代码超过 64KB 或链接脚本不对，确认用的是
  `STM32F103C8Tx_FLASH.ld`。
- 改了 CubeMX 配置后行为异常 → 重新 `GENERATE CODE`，并删掉 `build/` 再 `./build.sh`。

### 烧录 / ST-LINK 相关
- `st-info --probe` 没设备：
  - 检查 USB 线（有些线只供电不传数据）。
  - 检查 SWD 接线：SWDIO、SWCLK、GND、3V3 四根。
  - udev 权限，见第 5 节；先别用 sudo。
- OpenOCD `Error: open failed` / `LIBUSB_ERROR_ACCESS` → udev 权限问题。
- OpenOCD `Error: init mode failed (unable to connect to the target)`：
  - 目标板没供电 / BOOT0 跳线不对。
  - 把 BOOT0 接 GND（从 Flash 启动）。
  - 加 `-c "reset_config srst_only srst_nogate"`，或按住复位再连。
  - 有些克隆 ST-LINK 固件老旧，用 `stlink` 包的 `st-info --probe` 看版本，
    必要时用 `st-flash` 代替 OpenOCD 试试。
- 烧录成功但灯不闪：
  - 确认 LED 真的在 PC13（看原理图！）。
  - 确认 `main.c` 的 USER CODE 区写了 `HAL_GPIO_TogglePin + HAL_Delay`。
  - 确认 CubeMX 里 PC13 配成了 `GPIO_Output`。
  - 确认烧录后复位运行（`flash.sh` 里带 `reset`）。

### OpenOCD 找不到 cfg 文件
- 路径是相对 OpenOCD 的 scripts 目录（`/usr/share/openocd/scripts/`）。
  用 `interface/stlink.cfg`、`target/stm32f1x.cfg` 这种相对名即可，
  不要写绝对路径。
