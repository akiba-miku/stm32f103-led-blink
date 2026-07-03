# 第五天实验：串口终端温湿度界面

## 实验目标

用 STM32F103C8T6 每 1 秒采集一次 DHT11 温湿度，并显示在“无线接入系统”主界面中。

## 当前实现

由于当前没有实际液晶屏/大彩串口屏硬件，本工程使用串口终端作为显示界面：

1. USART1 初始化为 `115200 8N1`。
2. DHT11 DATA 接 PC15。
3. 一个 RTOS 任务每秒读取一次 DHT11。
4. 每次读取后通过 `printf` 刷新终端界面。
5. PC13 LED 每秒翻转一次，作为程序运行指示。

## 运行命令

烧录：

```bash
./build.sh
./flash.sh
```

监听串口：

```bash
picocom -b 115200 /dev/ttyACM0
```

退出 `picocom`：

```text
Ctrl-A
Ctrl-X
```

## 结果现象

终端每秒刷新一次：

```text
Wireless Access System
Temperature
Humidity
Status
Runtime
```

如果传感器正常，显示温度和湿度；如果没有响应，显示 DHT11 错误码。
