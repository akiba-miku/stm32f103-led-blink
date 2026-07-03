# STM32F103C8T6 第五天：串口终端无线接入系统界面

本工程用于完成第五天温湿度显示实验的替代演示版本：

- 设计“Wireless Access System”主界面
- STM32 每 1 秒采集一次 DHT11 温湿度
- 通过 USART1 将主界面和温湿度刷新到串口终端
- PC13 LED 每秒翻转一次，表示程序正在运行

说明：当前没有实际大彩串口屏/液晶屏硬件，因此本版本不再发送大彩屏二进制绘图指令，
而是在 `picocom` 终端中显示可拍摄的文字界面。这样可以避免乱码，并能直接录制实验现象。

## 硬件连接

| 模块 | STM32F103C8T6 引脚 | 说明 |
| --- | --- | --- |
| DHT11 DATA | PC15 | 当前工程使用 PC15 |
| 串口 TX | PA9 / USART1_TX | 输出终端界面 |
| 串口 RX | PA10 / USART1_RX | 当前演示不依赖输入 |
| GND | GND | 串口模块、DHT11、开发板共地 |
| Blue Pill LED | PC13 | 低电平点亮，每秒翻转一次 |

串口参数：

```text
USART1: 115200 8N1
```

## 文件说明

```text
Core/Src/main.c       主程序，1 秒采集 DHT11 并打印终端界面
Core/Src/dht11.c      DHT11 驱动，DATA=PC15
Core/Src/uart.c       USART1，115200 8N1
Core/Src/rtos.c       简易 RTOS，提供 1ms tick 和任务延时
docs/day5-serial-ui-dht11.md  实验说明
```

## 编译

```bash
./build.sh
```

或：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## 烧录

接好 DAPLink/CMSIS-DAP 后运行：

```bash
./flash.sh
```

## 查看结果

退出旧的 `picocom`：

```text
Ctrl-A
Ctrl-X
```

重新打开串口：

```bash
picocom -b 115200 /dev/ttyACM0
```

终端会每秒刷新一次，显示类似：

```text
+--------------------------------------------------+
|              Wireless Access System              |
+--------------------------------------------------+
| Device : STM32F103C8T6 Blue Pill                 |
| Sensor : DHT11 on PC15                           |
| UART   : USART1 115200 8N1                       |
+--------------------------------------------------+
| Temperature :  28 C                              |
| Humidity    :  55 %RH                            |
| Status      : OK                                 |
+--------------------------------------------------+
| Refresh     : 1 second                           |
| Runtime     : 12 s                               |
+--------------------------------------------------+
```

如果 DHT11 未响应，状态行会显示 `DHT11 read failed, error=<n>`。

## 拍视频

视频里建议同时拍到：

- `picocom` 终端每秒刷新界面
- 开发板和 DHT11 实物
- PC13 LED 每秒变化
- 视频上标注自己的班级、姓名、学号
