# f103-spl-lab —— STM32F103C8T6 精英板 · 标准库（SPL）学习仓库

STM32F103C8T6 精英板（嘉立创 v1.3）外设驱动学习项目，**标准外设库（Standard Peripheral Library, SPL）**版本。

> 本仓库是三条学习线之一：同一套外设功能（LED → 串口 → 按键 → 蜂鸣器/继电器 → 温湿度 → 烟雾 → OLED → W25Q Flash → ESP8266 连 OneNET）分别用 **[标准库（本仓库）] / [HAL 库] / [寄存器]** 三种方式各实现一遍，用于对比理解 STM32 的三种开发范式。

## ⚡ 快速开始（克隆下来 → 点亮，5 分钟）

**1. 拿到代码（二选一）**
- 会 git：`git clone git@github.com:Karry-King-code/f103-spl-lab.git`（需先配好 SSH 密钥；没有就下载 ZIP）
- 不会 git：网页上点绿色 **Code** → **Download ZIP**。
  ⚠️ 文档文件名是中文（如 `教程-标准库.md`），解压请用 **Windows 自带解压或 7-Zip**，
  老牌解压工具可能把中文名解成乱码；用 git clone 则完全无此问题。

**2. 装软件（一次性）**：Keil MDK5 + STM32F1xx 器件包 + STM32CubeProgrammer，
详细步骤（装哪个版本、pack 放哪、踩过的坑）见 docs 里【环境搭建】章节。

**3. 打开工程并编译**：双击 ``MDK-ARM/f103-spl-lab.uvprojx`` → Keil 里点 **Rebuild（F7）**，
底部看到 `0 Error(s)` 即成功。

**4. 烧录（ST-Link 接板子，4 根线 SWD）**：
```bash
cd 标准库/f103-spl-lab/MDK-ARM
STM32_Programmer_CLI -c port=SWD -w Objects/f103-spl-lab.hex -v -rst
```
> ⚠️ 命令行里含中文的路径可能被 CubeProgrammer 吞掉（实测报 `invalid ELF file`、
> 路径变成 `Desktop//...`）。遇到就先把 hex 拷到纯英文目录（如 `D:/flash/`）再烧，
> 或者像上面一样先 `cd` 进工程目录用相对路径。

**5. 验证**：板上绿灯 1 秒闪 = 环境全通。之后每章照着做：
`User/chapters/chX_*.c` 是每章的完整代码（已烧录验证），
**整份复制覆盖 `User/main.c`** 再编译烧录，现象对照该章验证表。

---


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
| 05-任务五 温湿度 DHT11 | 单总线时序/DWT 微秒延时 | ✅ 上板验收 |
| 06-任务六 光敏（烟雾替身） | 开漏必须上拉坑 | ✅ 上板验收 |
| 07-任务七 OLED | 软件 I2C 位带（本章含完整驱动） | ✅ 上板验收 |
| 08-任务八 SPI Flash | W25Q16 实测+掉电不丢写测试 | ✅ 上板验收 |
| 09-任务九 WiFi 上云 | ESP8266 AT+手工 MQTT+OneNET | ✅ 上板验收 |

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

## 每章独立代码（想跑哪章，就把对应文件内容复制进 main.c 编译烧录）

| 任务 | 内容 | 代码文件 | 状态 |
|---|---|---|---|
| 一 | 点亮 LED 灯 | `User/chapters/ch1_led_1s.c` | ✅ 烧录验证 |
| 二 | 串口收发 | `User/chapters/ch2_uart_echo.c` | ✅ 烧录验证 |
| 三 | 按键控制 LED | `User/chapters/ch3_key_led.c` | ✅ 烧录验证 |
| 四 | 蜂鸣器和继电器 | `User/chapters/ch4_buzzer_relay.c` | ✅ 烧录验证 |
| 五+六 | 温湿度 + 光敏打印 | `User/chapters/ch5_6_dht11_light_spl.c` | ✅ 烧录验证 |
| 七 | OLED 显示 | 见上表文件（含 OLED 代码）| ✅ 烧录验证 |
| 九 | WiFi上云 ESP8266→OneNET | `User/chapters/ch9_wifi_onenet_spl.c` | ✅ 烧录验证+用户验收 |

**接线定案**：DHT11=VCC/3V3 + DAT/B14 + GND｜光敏=VCC/3V3 + DO/B12 + GND｜OLED=VCC·GND·SCL/B8·SDA/B9
**三腿差异**：光敏上拉输入 = `GPIO_Mode_IPU`（标准库）/ `Pull=GPIO_PULLUP`（HAL）/ `CRH=0x8 且 ODR=1`（寄存器）
