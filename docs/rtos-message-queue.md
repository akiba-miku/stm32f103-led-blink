# 多线程消息队列按键实验

## 实验目标

- 编写一个多线程程序，至少开启 2 个线程。
- 线程 1 负责 PC13 板载 LED 点灯。
- 线程 2 负责通过 USART1 发送字符。
- 使用消息队列完成按键事件传递。
- 按不同按键时，LED 闪烁快慢不同，串口打印不同码值。

## 硬件连接

| 功能 | STM32F103C8T6 |
| --- | --- |
| 板载 LED | PC13 |
| 按键 1 | PA0，低电平触发 |
| 按键 2 | PA1，低电平触发 |
| USART1_TX | PA9 |
| USART1_RX | PA10 |
| GND | GND |

PA0 和 PA1 均配置为内部上拉，按键另一端接 GND。按下按键时分别触发 EXTI0 和 EXTI1 中断。

## 消息码值

| 按键 | 消息码值 | LED 响应 | 串口响应 |
| --- | --- | --- | --- |
| PA0 | `0x10` | 快速闪烁，周期 100 ms | 打印 `code=0x10` |
| PA1 | `0x20` | 慢速闪烁，周期 800 ms | 打印 `code=0x20` |

## 串口参数

```text
9600 8N1
```

## 代码结构

| 文件 | 作用 |
| --- | --- |
| `Core/Inc/rtos.h` | RTOS 任务、延时、信号量、消息队列接口 |
| `Core/Src/rtos.c` | 简易抢占式 RTOS，包含固定深度消息队列 |
| `Core/Src/main.c` | LED 线程、UART 线程、PA0/PA1 按键中断 |
| `Core/Src/uart.c` | USART1 初始化和串口输出 |

## 运行现象

上电后，串口会打印启动信息，然后 UART 线程每 1 秒发送一个字符：

```text
RTOS message queue demo
Thread1: PC13 LED, PA0 -> fast blink, PA1 -> slow blink
Thread2: USART1 send char, PA0 -> 0x10, PA1 -> 0x20
Buttons: PA0/PA1 active-low, UART1: 9600 8N1

[UART thread] send char: A
[UART thread] send char: B
```

按 PA0 后，LED 切到快速闪烁，串口打印：

```text
[UART thread] key message code=0x10
```

按 PA1 后，LED 切到慢速闪烁，串口打印：

```text
[UART thread] key message code=0x20
```
