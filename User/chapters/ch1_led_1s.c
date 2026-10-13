/**
  ******************************************************************************
  * @file    main.c
  * @brief   02 章：LED 1 秒闪烁（标准库）——正式版
  * @note    实测定稿（2026-10-04 照片+交替实验）：
  *            PC13 = 绿色 LED（低电平亮）← 本程序控制它
  *            PC14 = 继电器（低电平吸合，咔哒；想听咔哒就把 Pin_14 加回来）
  *            PWR  = 电源指示灯，常亮，软件控制不了
  *          电路：3.3V -> LED -> 4.7K -> 引脚，引脚输出低电平(灌电流)点亮
  *          页码注释约定：RM0008 = 《STM32F103xx参考手册(中文参考)》，页码为 PDF 阅读器页码
  ******************************************************************************
  */
#include "stm32f10x.h"

/* SysTick 精确毫秒延时：SysTick 是 Cortex-M3 内核外设（不在 ST 的 RM0008 里，
   见 PM0056《STM32F10x Cortex-M3 编程手册》第 4 章）。
   时钟源 HCLK/8，1ms = SystemCoreClock/8/1000 次计数。
   用 SystemCoreClockUpdate() 实测频率而不是写死 72000，HSE 失败回落 8M 时依然准。 */
void delay_ms(uint32_t ms)
{
    uint32_t tick = SystemCoreClock / 8 / 1000;   /* 1ms 要数的次数（时钟=HCLK/8=9MHz 时=9000） */
    SysTick->LOAD = tick - 1;                     /* LOAD=重装值寄存器：数到 0 自动重装；写"次数-1"因为计数含 0 */
    SysTick->VAL  = 0;                            /* VAL=当前计数值：写任意值清零，同时清掉 COUNTFLAG */
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk;      /* CTRL bit0=1 启动计数（CLKSOURCE 默认 0=外部时钟 HCLK/8） */
    while (ms--)
    {
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0)
        {                                         /* CTRL bit16=COUNTFLAG：数到 0 置 1，读后自动清零=正好过了 1ms */
        }
    }
    SysTick->CTRL = 0;                            /* 用完关掉，不留后台计数器 */
}

int main(void)
{
    GPIO_InitTypeDef gpio;

    SystemCoreClockUpdate();                                /* 实测系统时钟：底层=读 RCC_CFGR 的 SW/HPRE 位算出频率写入全局变量 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);   /* 底层=置 RCC->APB2ENR 的 bit4(IOPCEN)；地址=0x40021000+0x18=0x40021018；RM0008 中文版 PDF 第69页(组表)/第77页(位表) */

    gpio.GPIO_Pin   = GPIO_Pin_13;                          /* 选 13 号脚（库内部换算成 CRH 的 [23:20] 格子） */
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;                     /* 推挽输出 = CNF=00（通用推挽） */
    gpio.GPIO_Speed = GPIO_Speed_2MHz;                      /* MODE=10（2MHz）：1Hz 闪烁低速够 */
    GPIO_Init(GPIOC, &gpio);                                /* 底层=读改写 GPIOC->CRH 的 [23:20] 填 0b0010；RM0008 中文版 PDF 第113页(编码表)/第114页(CRH位表) */

    GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);             /* 底层=向 GPIOC->BSRR 写 1<<13 置位（高电平=灭）；PDF 第115页 */

    while (1)
    {
        GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_RESET);       /* 低电平 -> 亮：底层=向 GPIOC->BRR 写 1<<13 复位 */
        delay_ms(1000);
        GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);         /* 高电平 -> 灭：底层=向 GPIOC->BSRR 写 1<<13 置位 */
        delay_ms(1000);
    }
}
