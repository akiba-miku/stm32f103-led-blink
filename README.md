# STM32F103C8T6 第六天：LoRa 温湿度终端与网关

本工程用于完成第六天 LoRa 收发实验：

- 完成 LoRa 终端程序
- 完成 LoRa 网关程序
- 本机每 1 秒采集一次 DHT11 温湿度
- 通过 LoRa 透明串口模块互相发送温湿度数据
- 在串口主页面同时显示本终端温湿度和远端设备温湿度

工程一次构建会生成两个固件：

```text
build/stm32f103-led-blink-terminal.elf
build/stm32f103-led-blink-gateway.elf
```

## 硬件连接

### 调试串口

| 功能 | STM32 引脚 | 说明 |
| --- | --- | --- |
| USART1_TX | PA9 | 输出主页面到 `picocom` |
| USART1_RX | PA10 | 当前实验不依赖输入 |
| GND | GND | 与 DAPLink/USB 转串口共地 |

参数：

```text
USART1: 115200 8N1
```

### LoRa 透明串口模块

| LoRa 模块 | STM32 引脚 | 说明 |
| --- | --- | --- |
| TXD | PA3 / USART2_RX | LoRa 发给 STM32 |
| RXD | PA2 / USART2_TX | STM32 发给 LoRa |
| GND | GND | 必须共地 |
| VCC | 按模块要求 | 常见为 3.3V 或 5V，按模块丝印/手册 |

参数：

```text
USART2: 9600 8N1
```

代码按“透明串口 LoRa 模块”设计，例如 E32/E22/AS 系列透明传输模式。两个模块需要提前配成同一频点、同一空速、同一串口波特率。

### DHT11

| DHT11 | STM32 |
| --- | --- |
| DATA | PC15 |
| VCC | 3.3V 或 5V，按模块要求 |
| GND | GND |

## LoRa 数据帧

两个节点每秒发送一行 ASCII 帧：

```text
LH,<role>,<seq>,<temp>,<hum>,<error>\n
```

示例：

```text
LH,T,12,28,55,0
LH,G,12,29,58,0
```

`role` 为 `T` 表示终端，`G` 表示网关。

## 编译

```bash
./build.sh
```

或：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## 烧录

烧终端节点：

```bash
./flash.sh terminal
```

烧网关节点：

```bash
./flash.sh gateway
```

如果只有一块板，也可以先烧 `terminal` 拍本机界面，再烧 `gateway` 拍网关界面；有两块板和两套 LoRa 模块时，两个页面会显示对方的远端温湿度。

## 查看结果

```bash
picocom -b 115200 /dev/ttyACM0
```

退出 `picocom`：

```text
Ctrl-A
Ctrl-X
```

主页面示例：

```text
+-------------------------------------------------------------+
|                  LoRa Temperature Gateway                   |
+-------------------------------------------------------------+
| Node role : Terminal                                        |
| Debug UART: USART1 PA9/PA10 115200 8N1                     |
| LoRa UART : USART2 PA2/PA3   9600 8N1 transparent mode      |
| DHT11     : PC15                                            |
+----------+---------+----------+-----------------------------+
| Source   | Temp    | Humidity | Status                      |
+----------+---------+----------+-----------------------------+
| Local    |    28 C |     55 %RH | seq=12   age=0  s         |
| Remote   |    29 C |     58 %RH | seq=12   age=1  s         |
+----------+---------+----------+-----------------------------+
```

## 拍视频

视频里建议拍到：

- 终端或网关串口主页面
- Local 和 Remote 两行温湿度
- STM32、DHT11、LoRa 模块实物
- 视频上标注自己的班级、姓名、学号

如果没有收到远端 LoRa 数据，Remote 行会显示 `waiting`。
