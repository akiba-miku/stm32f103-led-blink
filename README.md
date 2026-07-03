# STM32F103C8T6 第六天：LoRa 温湿度终端与网关

本工程用于完成第六天 LoRa 收发实验：

- 完成 LoRa 终端程序
- 完成 LoRa 网关程序
- 本机每 1 秒采集一次 DHT11 温湿度
- 通过 LoRa 透明串口模块互相发送温湿度数据
- 在串口主页面同时显示本终端温湿度和远端设备温湿度
- 完成系统配置页面，支持切换终端/网关角色并修改参数

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
| USART1_RX | PA10 | 接收配置命令 |
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

`terminal` 和 `gateway` 只是默认启动角色不同。运行后可以在系统配置页面里按 `r` 随时切换终端/网关角色。

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

## 系统配置页面

在 `picocom` 里直接按键发送命令：

| 按键 | 功能 |
| --- | --- |
| `c` | 进入系统配置页面 |
| `m` | 返回主页面 |
| `r` | 切换 Terminal / Gateway 角色 |
| `+` | 采样周期增加 1 秒，最大 10 秒 |
| `-` | 采样周期减少 1 秒，最小 1 秒 |
| `b` | 切换 LoRa 串口波特率：9600 / 19200 / 115200 |

配置页会显示当前角色、远端预期角色、采样周期、LoRa 串口波特率、DHT11 引脚和 LoRa 引脚。
参数修改后立即生效。注意：如果修改 LoRa 串口波特率，外部 LoRa 模块本身也必须配置成相同波特率。

## 拍视频

视频里建议拍到：

- 终端或网关串口主页面
- 系统配置页面，展示角色切换和参数修改
- Local 和 Remote 两行温湿度
- STM32、DHT11、LoRa 模块实物
- 视频上标注自己的班级、姓名、学号

如果没有收到远端 LoRa 数据，Remote 行会显示 `waiting`。
