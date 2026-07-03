# 第六天实验：LoRa 终端与网关温湿度收发

## 实验目标

完成 LoRa 收发数据终端和网关程序，使主页面能同时显示本终端温湿度和远端设备温湿度。

## 程序结构

工程一次构建两个固件：

```text
stm32f103-led-blink-terminal.elf
stm32f103-led-blink-gateway.elf
```

CMake 通过 `NODE_ROLE=TERMINAL` 和 `NODE_ROLE=GATEWAY` 区分两个程序。主要代码：

```text
Core/Src/main.c       页面显示、DHT11 采集、LoRa 收发调度
Core/Src/lora.c       USART2 LoRa 透明串口驱动和帧解析
Core/Src/dht11.c      DHT11 驱动，DATA=PC15
Core/Src/uart.c       USART1 调试串口
```

## 通信协议

LoRa 模块使用透明传输模式。STM32 通过 USART2 发送 ASCII 文本帧：

```text
LH,<role>,<seq>,<temp>,<hum>,<error>\n
```

字段说明：

| 字段 | 说明 |
| --- | --- |
| `role` | `T` 终端，`G` 网关 |
| `seq` | 发送序号 |
| `temp` | 温度整数 |
| `hum` | 湿度整数 |
| `error` | DHT11 错误码，0 表示正常 |

接收端解析到对方角色的数据后，更新 Remote 行。

## 接线

### USART1 主页面

```text
PA9  -> USB 串口 RX
PA10 <- USB 串口 TX
GND  -> GND
```

或使用 DAPLink 的虚拟串口。

### USART2 LoRa

```text
PA2  -> LoRa RXD
PA3  <- LoRa TXD
GND  -> LoRa GND
VCC  -> 按模块要求供电
```

LoRa 模块参数：

```text
9600 8N1
透明传输模式
两个模块频点/地址/信道一致
```

### DHT11

```text
DATA -> PC15
VCC  -> 3.3V 或 5V
GND  -> GND
```

## 构建和烧录

```bash
./build.sh
./flash.sh terminal
./flash.sh gateway
```

`terminal` 和 `gateway` 分别烧到两块 STM32 板上。

## 结果

串口终端：

```bash
picocom -b 115200 /dev/ttyACM0
```

主页面会显示：

- Local：本机 DHT11 温湿度
- Remote：远端 LoRa 节点温湿度
- seq 和 age：远端数据序号与更新时间

如果没有收到远端数据，Remote 显示 `waiting`。
