# EasyLogger 多线程互斥锁实验

## 实验功能

本实验基于上一个 RTOS 邮箱队列温湿度实验继续实现：

- `sensor_thread`：每 1 秒读取一次 DHT11 温湿度，并输出 debug/error 日志。
- `uart_thread`：从邮箱队列接收温湿度，使用 EasyLogger 输出 info/error 日志。
- `led_thread`：从邮箱队列接收温度，根据温度控制 PC13 LED 快慢闪，并输出 info/error 日志。

EasyLogger 的 `elog_printf` 内部使用 RTOS 二值信号量作为互斥锁：

- 进入日志输出前调用 `rtos_sem_wait`。
- 整行日志输出完成后调用 `rtos_sem_signal`。
- RTOS 信号量支持多个等待线程 FIFO 排队，避免多个线程同时争用日志锁时丢失等待者。

这样多个线程同时打印时，串口上每一行日志都应保持完整，不会出现不同线程的半行日志互相穿插。

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

退出 picocom 使用 `Ctrl-a` 再按 `Ctrl-x`。

正常输出类似：

```text
[I] main:159 RTOS EasyLogger mutex demo start
[I] main:160 DHT11 PC15, UART1 PA9/PA10 9600 8N1
[I] main:161 temperature > 29 C: fast blink, otherwise slow blink
[D] sensor_thread:83 sensor thread sample temp=28 C humidity=55 %RH
[I] uart_thread:109 uart thread send temp=28 C humidity=55 %RH
[I] led_thread:139 led thread slow blink temp=28 C
```

判断日志互斥锁是否正常：每条日志从 `[` 开始，到换行结束，内容完整成行；不应看到不同线程日志在同一行中交叉。
