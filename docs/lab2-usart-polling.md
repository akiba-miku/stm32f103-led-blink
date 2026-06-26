# 实验二：USART 串口 DMA + IDLE 接收与回显

## 实验目标

- 使用 USART1 DMA 接收串口数据，并用 IDLE 中断判断一帧结束。
- 每次最长接收 10 个字符，收到空闲或满 10 字符后，将实际接收内容原样回显到串口。
- 通过 EasyLogger 分别输出 `debug/info/error` 三类日志，验证日志系统是否生效。
- 板载 LED 继续闪烁，用于确认程序在运行。

## 硬件连接

| STM32F103C8T6 | 串口模块 |
| --- | --- |
| PA9 / USART1_TX | RX |
| PA10 / USART1_RX | TX |
| GND | GND |

串口参数：`9600 8N1`。

## 代码结构

| 文件 | 作用 |
| --- | --- |
| `Core/Inc/uart.h` | USART1 接口声明 |
| `Core/Inc/elog.h` | EasyLogger 接口声明 |
| `Core/Src/elog.c` | EasyLogger 输出实现 |
| `Core/Src/uart.c` | USART1 初始化、发送、DMA 接收、IDLE 中断 |
| `Core/Src/main.c` | 实验主流程 |

## 运行现象

串口终端发送字符后，若总长度未满 10 个字符，停止发送会触发 IDLE 回显；若连续发送满 10 个字符，则满 10 后回显：

```text
ABCDEFGHIJ
```

同时程序会在启动后打印三行日志：

```text
[D] main:<line> debug log demo
[I] main:<line> info log demo
[E] main:<line> error log demo
```

同时板载 PC13 LED 继续以 500 ms 周期翻转，用于确认程序正在运行。
