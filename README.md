# f103-spl-lab —— STM32F103C8T6 精英板 · 标准库（SPL）学习仓库

STM32F103C8T6 精英板（嘉立创 v1.3）外设驱动学习项目，**标准外设库（Standard Peripheral Library, SPL）**版本。

> 本仓库是三条学习线之一：同一套外设功能（LED → 串口 → 按键 → 蜂鸣器/继电器 → 温湿度 → 烟雾 → OLED → W25Q Flash → ESP8266 连 OneNET）分别用 **[标准库（本仓库）] / [HAL 库] / [寄存器]** 三种方式各实现一遍，用于对比理解 STM32 的三种开发范式。

## 教程目录

| 章节 | 内容 | 状态 |
|---|---|---|
全部章节集成在一篇文档：[docs/教程-标准库.md](docs/教程-标准库.md)

| 章节 | 内容 | 状态 |
|---|---|---|
| 01 环境搭建与脚手架 | Keil/DFP 安装、工程结构、编译烧录全流程 | ✅ 上板验收 |
| 02 GPIO输出-LED 1s闪烁 | 原理图读图/灌电流/推挽输出/时钟门控/SysTick 延时 | ✅ 上板验收 |
| 03 GPIO输入-按键控LED | 上拉输入位级/读IDR/消抖/边沿检测/非阻塞分片 | ✅ 上板验收 |
| 04 USART-串口收发 | PA9/PA10 复用/BRR 换算/TXE-RXNE 标志/回显实测 | ✅ 上板验收 |

## 工程一览

- **主控**：STM32F103C8T6（Cortex-M3，72MHz，64KB Flash / 20KB RAM）
- **库**：ST 官方 STM32F10x Standard Peripheral Library **V3.5.0**（STSW-STM32054）
- **IDE**：Keil MDK5 + 编译器 ARMCC V5.06 (AC5) + 芯片包 Keil::STM32F1xx_DFP 2.3.0
- **烧录**：ST-Link（SWD）+ STM32CubeProgrammer

```
f103-spl-lab/
├─ User/                                # 用户代码（main、中断、外设配置头）
├─ Libraries/CMSIS/CM3/                 # CMSIS 内核层 + ST 官方设备层（stm32f10x.h / system / 启动文件）
├─ Libraries/STM32F10x_StdPeriph_Driver/# SPL 官方外设驱动（脚手架仅启用 rcc/gpio/misc，逐章追加）
├─ MDK-ARM/f103-spl-lab.uvprojx         # Keil 工程
└─ docs/                                # 教程文档（原理图/芯片手册不入库，见 01 章）
```

## 硬件资料说明

原理图与芯片手册因版权原因**保存在本地不入仓库**（`docs/schematics/`、`docs/datasheets/` 已在 .gitignore 排除）。板卡引脚分配速查表见 `docs/01-环境搭建与脚手架.md`。

## 免责声明

SPL V3.5.0 为 ST 官方历史库（已停止更新，官方现推 HAL/CubeMX），用于教学理解寄存器与库的映射关系。生产项目请使用 HAL/LL。

## 章节代码（每章独立，想跑哪章复制哪章）

> 一个工程同一时刻只编译一个 main.c。**方法：把 `User/chapters/`（HAL为 `Src/chapters/`）里对应章节的 .c 内容整体复制进 main.c → 编译 → 烧录**。各章完整讲解见 docs/教程文档。

| 任务编号 | 章节 | 代码文件 | 现象（验收证据） |
|---|---|---|---|
| 任务一 | LED 1s 闪烁 | ch1_led_1s.c | 绿灯亮1秒灭1秒循环 |
| 任务二 | 串口收发 | ch2_uart_echo.c | 串口发什么回什么（回显） |
| 任务三 | 按键控制 LED | ch3_key_led.c | 按一下 KEY1 开始 1s 闪烁，再按停止 |
| 任务四 | 按键控蜂鸣器/继电器 | ch4_buzzer_relay.c | KEY1 蜂鸣器响/停，KEY2 继电器吸合/释放 |
| 任务五 | 串口打印温湿度 | ch5_dht11.c | 串口每 2 秒打印温湿度 |
| 任务六 | 串口打印烟雾(光敏替) | ch6_light.c | 挡光打印报警，松开恢复 |
| 扩展 | OLED 显示 | ch8_oled_display.c | 屏幕显示环境信息+实时光敏 |

## ✅ 任务验收总表（2026-10-06 全部通过）

| 任务 | 内容 | 章节 | 状态 |
|---|---|---|---|
| 一 | LED 1s 闪烁 | 02 章 | ✅ 三腿验收 |
| 二 | 串口收发数据 | 04 章 | ✅ 三腿验收（回显实测） |
| 三 | 按键控制 LED | 03 章 | ✅ 三腿验收（用户亲测） |
| 四 | 按键控蜂鸣器/继电器 | 05 章 | ✅ 三腿验收（用户听声确认） |
| 五 | 串口打印温湿度 | 06 章 | ✅ **实测 27.6°C/49%（含卡死断电重启发现）** |
| 六 | 串口打印烟雾(光敏替身) | 07 章 | ✅ **屏幕 OK/DARK 实时联动（用户确认）** |
| 扩展 | OLED 显示 | 08 章 | ✅ 四行显示+三外设共存整合通过 |
