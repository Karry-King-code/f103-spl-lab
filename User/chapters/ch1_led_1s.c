/**
  ******************************************************************************
  * @file    main.c
  * @brief   02 章：LED 1 秒闪烁（标准库）——正式版
  * @note    实测定稿（2026-10-04 照片+交替实验）：
  *            PC13 = 绿色 LED（低电平亮）← 本程序控制它
  *            PC14 = 继电器（低电平吸合，咔哒；想听咔哒就把 Pin_14 加回来）
  *            PWR  = 电源指示灯，常亮，软件控制不了
  *          电路：3.3V -> LED -> 4.7K -> 引脚，引脚输出低电平(灌电流)点亮
  ******************************************************************************
  */
#include "stm32f10x.h"

/* SysTick 精确毫秒延时：时钟源 HCLK/8，1ms = SystemCoreClock/8/1000 次计数。
   用 SystemCoreClockUpdate() 实测频率而不是写死 72000，HSE 失败回落 8M 时依然准。 */
void delay_ms(uint32_t ms)
{
    uint32_t tick = SystemCoreClock / 8 / 1000;
    SysTick->LOAD = tick - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk;
    while (ms--)
    {
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0)
        {
        }
    }
    SysTick->CTRL = 0;
}

int main(void)
{
    GPIO_InitTypeDef gpio;

    SystemCoreClockUpdate();                                /* 实测系统时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);   /* 开 GPIOC 时钟（APB2） */

    gpio.GPIO_Pin   = GPIO_Pin_13;                          /* 只用 PC13（绿灯） */
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;                     /* 推挽输出 */
    gpio.GPIO_Speed = GPIO_Speed_2MHz;                      /* 1Hz 闪烁低速够 */
    GPIO_Init(GPIOC, &gpio);

    GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);             /* 上电先灭（高电平=灭） */

    while (1)
    {
        GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_RESET);       /* 低电平 -> 亮 */
        delay_ms(1000);
        GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);         /* 高电平 -> 灭 */
        delay_ms(1000);
    }
}
