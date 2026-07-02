# 事件架构温湿度实验

## 实验功能

本实验用基于事件的架构组织程序：

- 按键扫描模块：TIM2 每 10ms 调用一次按键扫描，PA0/PA1 使用内部上拉，低电平按下。
- 温湿度采集事件：TIM2 每累计 100 次 10ms tick，投递一次 1 秒采集事件。
- 事件线程：处理采集事件和按键事件。
- UART 线程：收到 UART 事件后，通过 USART1 打印当前温湿度。
- LED 线程：收到湿度事件后，根据湿度高低调整 PC13 闪烁速度。

中断里只投递事件，不直接读取 DHT11，也不直接做串口输出。

## 事件流

```text
TIM2 10ms tick
  ├─ 按键扫描模块检测 PA0/PA1 松开 -> APP_EVT_KEY_RELEASED
  └─ 每 100 次 tick -> APP_EVT_SENSOR_SAMPLE_DUE

event_thread
  ├─ SENSOR_SAMPLE_DUE -> 读取 DHT11 -> LED_EVT_HUMIDITY
  └─ KEY_RELEASED -> 取当前温湿度 -> UART_EVT_PRINT_SAMPLE

uart_thread -> 打印当前温湿度
led_thread  -> 湿度 >= 60%RH 快闪，否则慢闪
```

## 接线

| 模块 | STM32 引脚 |
| --- | --- |
| DHT11 DATA | PC15 |
| DHT11 VCC | 3.3V |
| DHT11 GND | GND |
| KEY0 | PA0 -> 按键 -> GND |
| KEY1 | PA1 -> 按键 -> GND |
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

启动输出：

```text
Event-driven DHT11 key demo
TIM2: key scan every 10 ms, DHT11 sample every 1 second
Press and release PA0/PA1 to print current temperature/humidity
LED: humidity >= 60%RH fast blink, otherwise slow blink
DHT11 DATA: PC15, UART1: PA9 TX / PA10 RX, 9600 8N1
```

按键后输出示例：

```text
[UART] key=0x10 temp=28 C humidity=55 %RH
[UART] key=0x20 temp=28 C humidity=55 %RH
```

如果还没有采集到有效数据，或 DHT11 读取失败，串口会输出对应提示。
