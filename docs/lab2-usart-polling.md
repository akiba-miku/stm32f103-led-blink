# 实验二：USART 串口发送与轮询接收

## 实验目标

- 使用 USART1 每 1 秒发送一次自己的姓名汉语拼音。
- 使用轮询方式接收串口数据。
- 收到数据后通过串口回显，证明接收功能正常。

## 硬件连接

| STM32F103C8T6 | 串口模块 |
| --- | --- |
| PA9 / USART1_TX | RX |
| PA10 / USART1_RX | TX |
| GND | GND |

串口参数：`115200 8N1`。

## 代码结构

| 文件 | 作用 |
| --- | --- |
| `Core/Inc/lab2_config.h` | 配置姓名拼音 |
| `Core/Inc/uart.h` | USART1 接口声明 |
| `Core/Src/uart.c` | USART1 初始化、发送、轮询接收 |
| `Core/Src/main.c` | 实验主流程 |

提交前请把 `Core/Inc/lab2_config.h` 中的：

```c
#define LAB2_NAME_PINYIN "YOUR_NAME_PINYIN"
```

改成自己的姓名拼音，例如：

```c
#define LAB2_NAME_PINYIN "Zhang San"
```

## 运行现象

串口终端每 1 秒收到一行：

```text
Name pinyin: Zhang San
```

在串口终端发送任意字符，单片机会回显：

```text
RX: A
```

同时板载 PC13 LED 继续以 500 ms 周期翻转，用于确认程序正在运行。
