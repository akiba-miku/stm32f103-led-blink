# STM32F103C8T6 EasyLogger 多线程互斥锁实验

把传统的 **Keil µVision5 实验** 改造成 **Arch Linux 原生嵌入式开发流程**。
目标功能：在上一个 DHT11 邮箱队列实验中加入 EasyLogger 日志系统，并在
EasyLogger 输出路径中加入互斥锁，保证多个线程同时输出日志时每一行日志完整、不交叉。

当前工程包含 3 个线程：DHT11 采集线程、串口日志线程、LED 控制线程。采集线程仍然
每秒通过邮箱队列发送温湿度；不同线程都会调用 EasyLogger 输出日志。详见
[docs/easylogger-mutex-threads.md](docs/easylogger-mutex-threads.md)。

| 项目 | 值 |
| --- | --- |
| MCU | **STM32F103C8T6**（Cortex-M3, 64KB Flash, 20KB RAM, 封装 LQFP48） |
| 开发板 | Blue Pill（最常见的小蓝板） |
| LED 引脚 | **PC13**（Blue Pill 板载 LED，**低电平点亮 / active-low**） |
| DHT11 DATA | **PC15** |
| 串口 | **USART1：PA9 TX / PA10 RX，9600 8N1** |
| 日志 | **EasyLogger + RTOS 二值信号量互斥锁** |
| 调试器 | **ST-LINK V2** |
| 工程类型 | Arch Linux 原生 **CMake + arm-none-eabi-gcc** 裸机工程 |
| OpenOCD target | `target/stm32f1x.cfg` |
| OpenOCD interface | `interface/stlink.cfg` |

> ⚠️ **PC13 是根据“Blue Pill”这一板型推断的**。如果你的板子不是标准 Blue Pill，
> 请先看原理图确认 LED 引脚，再改 `Core/Src/main.c` 与本表。详见
> [docs/linux-stm32-workflow.md](docs/linux-stm32-workflow.md)。

---

## 一、为什么 Linux 不用 Keil5 / µVision

- **Keil µVision5（MDK-ARM）只有 Windows 版本**，没有 Linux 原生版。Keil 本质是
  Windows GUI + ARMCC/ARMClang 编译器 + Windows 调试后端，强行在 Linux 上跑要靠
  Wine，体验差且不稳定，**不是 Linux 原生方案**。
- Linux 上等价、且完全开源/免费的链路是：

  ```
  STM32CubeMX（图形化配置 + 生成代码）
        │  生成 CMake 工程 / HAL 库 / 启动文件 / 链接脚本
        ▼
  arm-none-eabi-gcc + newlib   （编译，等价于 ARMCC）
        │  + CMake + Ninja     （构建系统，等价于 Keil 的 IDE 构建）
        ▼
  OpenOCD + ST-LINK            （烧录 + 调试，等价于 Keil 的 Download/Debug）
        │  + arm-none-eabi-gdb （命令行/图形调试）
        ▼
  开发板运行多线程温湿度采集、LED 告警和互斥日志输出
  ```

- 课程要求里的 “MDK5” 在 Linux 下由 **arm-none-eabi-gcc 工具链** 替代，
  “STM32CubeMX” 继续使用（它有官方 Linux 版）。

### 两条 Linux 路线

| 路线 | 说明 | 本工程采用 |
| --- | --- | --- |
| **A. STM32CubeIDE** | ST 官方 Eclipse IDE，自带 CubeMX + GCC + GDB，开箱即用，适合“点点点” | 备选 |
| **B. STM32CubeMX + CMake + GCC + OpenOCD** | 命令行 / VS Code，可复现、可写进 Git、可 CI，最“Linux 原生” | ✅ **本工程** |

---

## 二、安装工具链（Arch Linux）

官方仓库的部分：

```bash
sudo pacman -S git cmake ninja arm-none-eabi-gcc arm-none-eabi-newlib openocd stlink dfu-util
```

> 本机已装：`cmake` `ninja` `make` `git`。需要补装的是
> `arm-none-eabi-gcc` `arm-none-eabi-newlib` `openocd` `stlink` `dfu-util`。
> 在 Claude Code 里可以直接输入 `! sudo pacman -S ...` 来运行（会让你输密码）。

STM32CubeMX / CubeProgrammer 在 **AUR**（用 paru）：

```bash
paru -S stm32cubemx          # 图形化配置 + 生成 CMake 工程（必需）
paru -S stm32cubeprog        # STM32CubeProgrammer（可选，另一种烧录方式）
# 备选：paru -S stm32cubeide  # 想用官方 IDE 的话
```

验证：

```bash
arm-none-eabi-gcc --version
openocd --version
st-info --probe        # 插上 ST-LINK 后能看到设备
```

---

## 三、当前工程状态

本目录已经包含可直接构建和烧录的 STM32F103C8T6 EasyLogger 多线程互斥锁工程：

```
stm32f103-led-blink/
├── CMakeLists.txt
├── cmake/arm-none-eabi.cmake
├── Core/Src/main.c                # 三个 RTOS 线程和邮箱队列使用逻辑
├── Core/Src/rtos.c                # 简易 RTOS + 邮箱队列
├── Core/Src/dht11.c               # DHT11 PC15 驱动
├── Core/Src/elog.c                # EasyLogger + 互斥锁保护
├── Core/Src/uart.c                # USART1 输出
├── startup_stm32f103xb.s
├── STM32F103C8Tx_FLASH.ld
├── build.sh
└── flash.sh
```

这里采用寄存器裸机方式实现，不依赖 HAL 包下载；`SysTick` 每 1ms 中断一次，
RTOS 调度 3 个线程：温湿度采集线程、串口日志线程、LED 控制线程。

如果课程必须提交 STM32CubeMX 截图或 `.ioc` 文件，可再按下面步骤用 CubeMX 生成
同名 CMake 工程，然后把本工程的 `Core/Src` 与 `Core/Inc` 代码迁入生成工程。

## 四、用 STM32CubeMX 重新生成工程（可选）

> 启动文件 `startup_stm32f103xb.s` 和链接脚本 `STM32F103C8Tx_FLASH.ld` **由 CubeMX 生成**，
> 不要手写、也不要从别处乱抄（型号不匹配会编译/烧录失败）。

1. 打开 `stm32cubemx`，**New Project → 选 MCU `STM32F103C8Tx`**。
2. **Pinout & Configuration**：
   - 点引脚 **PC13** → 选 **`GPIO_Output`**。
   - 左侧 `System Core → GPIO`：把 PC13 的 `GPIO output level` 设为 `High`（active-low 灯先灭），
     `Mode = Output Push Pull`，`Max output speed = Low` 即可。
   - 想给引脚起名可设 User Label = `LED`（可选）。
3. **Clock Configuration**：用默认内部 HCLK 即可（8MHz HSI 也能闪灯）。
   想跑 72MHz 就接 8MHz HSE → PLL ×9，本实验非必需。
4. **Project Manager 选项卡**：
   - `Project Name` = `stm32f103-led-blink`
   - `Project Location` = **本目录的上一级**（让它生成到 `~/Projects/`）
   - **`Toolchain / IDE` = `CMake`** ← 关键！这样才生成 CMake 工程而不是 Keil 工程
   - `Code Generator` 勾选 **“Copy only the necessary library files”**。
5. **GENERATE CODE**。

生成后目录大致是：

```
stm32f103-led-blink/
├── CMakeLists.txt                  # CubeMX 生成（顶层）
├── cmake/
│   ├── gcc-arm-none-eabi.cmake     # GCC 工具链文件（CubeMX 生成）
│   └── stm32cubemx/CMakeLists.txt
├── Core/Inc/ , Core/Src/main.c     # 你的代码写在 main.c 的 USER CODE 区
├── Drivers/                        # HAL 库（CubeMX 生成）
├── startup_stm32f103xb.s           # 启动文件（CubeMX 生成）
├── STM32F103C8Tx_FLASH.ld          # 链接脚本（CubeMX 生成）
├── stm32f103-led-blink.ioc         # CubeMX 工程文件
├── README.md   build.sh   flash.sh   .gitignore   docs/   ← 本仓库已提供
└── build/                          # build.sh 生成（已 gitignore）
```

> 让 CubeMX 直接生成到这个目录即可，本仓库已放好的
> `README.md / build.sh / flash.sh / .gitignore / docs/` 不会被覆盖。

---

## 五、主要代码位置

- `Core/Src/main.c`：创建温湿度采集线程、串口线程、LED 线程。
- `Core/Src/rtos.c`：实现任务调度、信号量、消息队列和邮箱队列；信号量支持多等待者 FIFO 唤醒。
- `Core/Src/elog.c`：在 EasyLogger 输出路径中用 RTOS 二值信号量加锁。
- `Core/Src/dht11.c`：DHT11 驱动，DATA 接 PC15。
- `Core/Src/uart.c`：USART1 初始化和输出。

本次实验的邮箱队列接口是 `rtos_mailbox_create`、`rtos_mailbox_send`、
`rtos_mailbox_try_recv`；日志互斥锁在 `elog_printf` 内部自动使用。

---

## 六、编译

```bash
./build.sh
```

它执行（CMake + Ninja + GCC 交叉编译）：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

成功后在 `build/` 下得到 `stm32f103-led-blink.elf`（以及 map 等）。
查看大小：`arm-none-eabi-size build/*.elf`。

---

## 七、烧录

接好 ST-LINK V2（SWD：SWDIO/SWCLK/GND/3V3），然后：

```bash
./flash.sh
```

它用 OpenOCD：

```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
        -c "program build/<工程名>.elf verify reset exit"
```

`flash.sh` 会自动找 `build/*.elf`，无需手填名字。

**另一种烧录方式（stlink 工具，不经 OpenOCD）**：

```bash
arm-none-eabi-objcopy -O binary build/*.elf build/firmware.bin
st-flash write build/firmware.bin 0x08000000
```

---

## 八、调试（可选）

一个终端开 OpenOCD GDB server：

```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg
```

另一个终端：

```bash
arm-none-eabi-gdb build/*.elf
(gdb) target extended-remote localhost:3333
(gdb) load
(gdb) monitor reset halt
(gdb) continue
```

VS Code 用 **Cortex-Debug** 扩展可图形化调试（见下）。

---

## 九、验证实验结果

1. 烧录后开发板自动复位运行。
2. 串口监听 `9600 8N1`，应看到 `[I]`、`[D]`、`[E]` 等 EasyLogger 格式日志。
3. 温度大于 29 摄氏度时，PC13 LED 快闪；温度小于等于 29 摄氏度时慢闪。
4. 多个线程连续输出时，每行日志应完整显示，不应出现半行混杂。
5. 若串口显示 `DHT11 read failed`，优先检查 PC15、3.3V、GND 和 DATA 上拉。

---

## 十、VS Code 扩展（可选）

- **C/C++**（ms-vscode.cpptools）
- **CMake Tools**（ms-vscode.cmake-tools）
- **Cortex-Debug**（marus25.cortex-debug）——SWD 图形调试
- Arm Keil Studio Pack（可选）

---

## 十一、出问题怎么排查

见 [docs/linux-stm32-workflow.md](docs/linux-stm32-workflow.md) 的“故障排查”一节
（ST-LINK 权限 / udev、OpenOCD 找不到设备、编译报错等）。

## 需要你确认的点（TODO）

- [ ] 确认板子确实是 **Blue Pill**、LED 在 **PC13**（不是的话改 `main.c` 和本 README）。
- [ ] DHT11 DATA 已接 **PC15**，串口使用 **PA9/PA10**，并且共地。
- [ ] 装好 `arm-none-eabi-gcc / newlib / openocd / stlink`（见第二节）。
