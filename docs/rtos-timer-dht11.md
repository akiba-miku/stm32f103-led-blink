# RTOS 定时器温湿度实验

## 实验功能

本实验在 STM32F103C8T6 Blue Pill 上运行 3 个线程，并使用 TIM2 硬件定时器触发采集：

- `TIM2_IRQHandler`：每 1 秒进入一次中断，只释放采集信号量。
- `sensor_thread`：等待 TIM2 信号量，收到后读取 DHT11 温湿度。
- `uart_thread`：接收采集线程发来的温湿度，通过 USART1 输出。
- `led_thread`：接收采集线程发来的温度，温度大于 29 摄氏度时 PC13 快闪，否则慢闪。

DHT11 的读取时序比较长，所以中断里不直接读 DHT11，只通知采集线程去读。
采集线程读取完成后，通过邮箱队列分别把同一份数据发给串口发送线程和 LED 线程。

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

如果使用裸 DHT11 传感器，DATA 和 3.3V 之间需要上拉电阻；很多 DHT11 模块板上已经带上拉。

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
RTOS timer DHT11 demo
TIM2: trigger DHT11 sampling every 1 second
UART thread: print temperature and humidity via USART1
LED thread: PC13 fast blink when temp > 29 C, otherwise slow blink
DHT11 DATA: PC15, UART1: PA9 TX / PA10 RX, 9600 8N1

[UART thread] temp=28 C, humidity=55 %RH
[UART thread] temp=28 C, humidity=55 %RH
```

温度大于 29 摄氏度时，PC13 LED 约每 100ms 翻转一次；温度为 29 摄氏度或更低时，约每 800ms 翻转一次。
