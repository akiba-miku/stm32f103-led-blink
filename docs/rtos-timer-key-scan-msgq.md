# RTOS 定时器按键扫描消息队列实验

## 实验功能

本实验在 STM32F103C8T6 Blue Pill 上运行 2 个线程，并使用 TIM2 硬件定时器扫描按键：

- `TIM2_IRQHandler`：每 10ms 扫描一次 PA0/PA1，做简单消抖。
- 检测到按键稳定松开时，向 LED 线程和 UART 线程分别发送消息。
- `led_thread`：接收按键消息，PA0 松开后 PC13 快闪，PA1 松开后 PC13 慢闪。
- `uart_thread`：每 1 秒发送一个字符；收到按键消息后打印不同码值。

消息队列使用 `rtos_msgq_create`、`rtos_msgq_send`、`rtos_msgq_try_recv`。

## 消息码

| 按键事件 | 消息码 | LED 响应 | UART 响应 |
| --- | --- | --- | --- |
| PA0 松开 | `0x10` | 快闪，约 100ms 翻转一次 | 打印 `0x10` |
| PA1 松开 | `0x20` | 慢闪，约 800ms 翻转一次 | 打印 `0x20` |

## 接线

| 模块 | STM32 引脚 |
| --- | --- |
| KEY0 | PA0 -> 按键 -> GND |
| KEY1 | PA1 -> 按键 -> GND |
| 板载 LED | PC13 |
| USART1 TX | PA9 -> USB 转串口 RX |
| USART1 RX | PA10 -> USB 转串口 TX |
| GND | 与 USB 转串口 GND 共地 |

PA0/PA1 在程序中配置为输入上拉，所以按键按下为低电平，松开为高电平。

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
RTOS timer key scan message queue demo
TIM2 scans PA0/PA1 every 10 ms and sends messages on key release
PA0 release -> code 0x10, LED fast blink
PA1 release -> code 0x20, LED slow blink
UART1: PA9 TX, PA10 RX, 9600 8N1

[UART thread] send char: A
[UART thread] send char: B
[UART thread] key released, code=0x10
[UART thread] key released, code=0x20
```
