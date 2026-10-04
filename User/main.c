/**
  ******************************************************************************
  * @file    main.c
  * @brief   02 章：LED 1 秒闪烁（标准库）
  * @note    LED1=PC14（绿，低电平亮，与继电器共用）；LED2=PC13（红，低电平亮）
  *          电路（原理图 P3 LED and Key Driver 区）：
  *          3V3 -> LED -> 4.7K -> 引脚，引脚输出低电平(灌电流)时点亮。
  *          教程：docs/01-环境搭建与脚手架-标准库.md 第 02 章
  ******************************************************************************
  */
#include "stm32f10x.h"

/* ---------- SysTick 精确毫秒延时 ----------
 * SysTick 是 Cortex-M3 内核自带的 24 位递减计数器，不占用芯片外设。
 * 时钟源选 HCLK/8：计数一次 = 8/SystemCoreClock 秒。
 * 1ms 需要计数 SystemCoreClock/8/1000 次。
 * 不写死 72000 而用实测值：原理图未画晶振，HSE 起振失败时
 * SystemInit() 会静默回落 HSI 8MHz，SystemCoreClockUpdate()
 * 读 RCC 寄存器现场计算，72M/8M 两种情况都准确。
 */
void delay_ms(uint32_t ms)
{
    uint32_t tick = SystemCoreClock / 8 / 1000;   /* 1ms 的计数次数 */
    SysTick->LOAD = tick - 1;                     /* 重装值 */
    SysTick->VAL  = 0;                            /* 清当前计数值 */
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk;      /* 使能（CLKSOURCE=0 -> HCLK/8） */
    while (ms--)
    {
        /* COUNTFLAG 计到 0 由硬件置 1，CPU 读一次自动清零 */
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0)
        {
        }
    }
    SysTick->CTRL = 0;                            /* 用完关闭 */
}

int main(void)
{
    GPIO_InitTypeDef gpio;                        /* 标准库的"配置单"结构体 */

    /* 0) 实测当前系统时钟写入 SystemCoreClock 变量（delay 依赖它） */
    SystemCoreClockUpdate();

    /* 1) 打开 GPIOC 时钟（APB2）——不开时钟，下面的配置写不进去 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    /* 2) 填"配置单"：PC13+PC14 推挽输出 2MHz */
    gpio.GPIO_Pin   = GPIO_Pin_13 | GPIO_Pin_14;  /* LED2=PC13, LED1=PC14 */
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;           /* 推挽输出 */
    gpio.GPIO_Speed = GPIO_Speed_2MHz;            /* 1Hz 闪烁，低速即可 */

    /* 3) 应用配置单（内部写 GPIOC 的 CRH 寄存器 [15:0]每脚4位） */
    GPIO_Init(GPIOC, &gpio);

    /* 4) 上电先输出高电平（两灯灭），再进闪烁循环 */
    GPIO_WriteBit(GPIOC, GPIO_Pin_13 | GPIO_Pin_14, Bit_SET);

    while (1)
    {
        /* 低电平 -> 两灯亮（PC14 同时驱动继电器吸合，会"咔哒"一声） */
        GPIO_WriteBit(GPIOC, GPIO_Pin_13 | GPIO_Pin_14, Bit_RESET);
        delay_ms(1000);                           /* 亮 1 秒 */

        /* 高电平 -> 灭 */
        GPIO_WriteBit(GPIOC, GPIO_Pin_13 | GPIO_Pin_14, Bit_SET);
        delay_ms(1000);                           /* 灭 1 秒 */
    }
}
