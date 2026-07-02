# 多线程按键信号同步实验

## 实验目标

- 编写一个多线程程序，至少开启 2 个线程。
- 线程 1 负责 PC13 板载 LED 点灯。
- 线程 2 负责通过 USART1 发送字符。
- 使用信号量进行同步。
- 当 PA0 按键按下时，两个线程分别作出响应。

## 硬件连接

| 功能 | STM32F103C8T6 |
| --- | --- |
| 板载 LED | PC13 |
| 按键输入 | PA0，低电平触发 |
| USART1_TX | PA9 |
| USART1_RX | PA10 |
| GND | GND |

PA0 已配置为内部上拉，按键另一端接 GND。按下按键时触发 EXTI0 中断。

## 串口参数

```text
9600 8N1
```

## 代码结构

| 文件 | 作用 |
| --- | --- |
| `Core/Inc/rtos.h` | RTOS 任务、延时、信号量接口 |
| `Core/Src/rtos.c` | 简易抢占式 RTOS，使用 SysTick 和 PendSV 调度 |
| `Core/Src/main.c` | LED 线程、UART 线程、PA0 按键中断 |
| `Core/Src/uart.c` | USART1 初始化和串口输出 |

## 运行现象

上电后，串口会打印启动信息，然后 UART 线程每 1 秒发送一个字符：

```text
RTOS semaphore sync demo
Thread1: PC13 LED blink, button -> rapid blink
Thread2: USART1 send char, button -> response message
Button: PA0 active-low, UART1: 9600 8N1

[UART thread] send char: A
[UART thread] send char: B
```

PC13 LED 由 LED 线程每 500 ms 翻转一次。按下 PA0 后：

- LED 线程收到信号，PC13 快速闪烁。
- UART 线程收到信号，串口打印按键响应信息。

```text
[UART thread] button signal received at 5234 ms
```
