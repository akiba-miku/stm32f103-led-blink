# DHT11 温湿度串口打印实验

## 实验目标

- 编写 DHT11 驱动代码。
- 通过 USART1 每 1 秒打印一次当前湿度和温度。
- PC13 板载 LED 每 500 ms 翻转一次，用于确认程序正在运行。

## 硬件连接

| STM32F103C8T6 | DHT11 |
| --- | --- |
| PC15 | DATA |
| 3V3 | VCC |
| GND | GND |

如果使用裸 DHT11 传感器，DATA 和 VCC 之间需要接 4.7k 到 10k 上拉电阻；常见 DHT11 模块通常已经自带上拉电阻。

## 串口参数

| 项目 | 值 |
| --- | --- |
| USART | USART1 |
| TX | PA9 |
| RX | PA10 |
| 波特率 | 9600 |
| 数据格式 | 8N1 |

## 运行现象

打开串口后会看到启动信息，然后每 1 秒输出一次温湿度：

```text
DHT11 temperature/humidity demo
DHT11 DATA: PC15, UART1: 9600 8N1
Humidity=55%RH Temperature=26C
Humidity=55%RH Temperature=26C
```

如果接线或传感器响应异常，会输出错误码和诊断信息。
