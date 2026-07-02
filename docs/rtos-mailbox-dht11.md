# RTOS 邮箱队列温湿度实验

## 实验功能

本实验在 STM32F103C8T6 Blue Pill 上运行 3 个线程：

- `sensor_thread`：每 1 秒读取一次 DHT11 温湿度。
- `uart_thread`：从邮箱队列接收温湿度，并通过 USART1 打印。
- `led_thread`：从邮箱队列接收温度，温度大于 29 摄氏度时 PC13 快闪，否则慢闪。

线程间通信使用 `rtos_mailbox_create`、`rtos_mailbox_send`、
`rtos_mailbox_try_recv` 实现的邮箱队列。采集线程会把同一份温湿度邮件分别发送给
串口线程和 LED 线程。

## 接线

| 模块 | STM32 引脚 |
| --- | --- |
| DHT11 DATA | PC15 |
| DHT11 VCC | 3.3V |
| DHT11 GND | GND |
| 板载 LED | PC13 |
| USART1 TX | PA9 -> USB 转串口 RX |
| USART1 RX | PA10 -> USB 转串口 TX |
| GND | 与 USB 转串口 GND 共地 |

如果使用的是裸 DHT11 传感器，DATA 和 3.3V 之间需要上拉电阻；很多 DHT11 模块板上已经带上拉。

## 编译和烧录

```bash
cd /home/arsenova-arch/Projects/stm32f103-led-blink
./build.sh
./flash.sh
```

## 查看串口输出

```bash
picocom -b 9600 /dev/ttyACM0
```

如果你的 USB 转串口是 `/dev/ttyUSB0`，把命令里的设备名换成 `/dev/ttyUSB0`。
退出 picocom 使用 `Ctrl-a` 再按 `Ctrl-x`。

正常输出示例：

```text
RTOS mailbox DHT11 demo
Sensor thread: read DHT11 on PC15 every 1 second
UART thread: print temperature and humidity via USART1
LED thread: PC13 fast blink when temp > 29 C, otherwise slow blink
UART1: PA9 TX, PA10 RX, 9600 8N1

[UART] temp=28 C, humidity=55 %RH
[UART] temp=28 C, humidity=55 %RH
```

温度大于 29 摄氏度时，PC13 LED 约每 100ms 翻转一次；温度小于等于 29 摄氏度或读取失败时，约每 800ms 翻转一次。
